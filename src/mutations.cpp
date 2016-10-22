#include "mutations.h"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"


using namespace llvm;
using namespace std;

void mutateGlobalConsructorNumElements(GlobalVariable *var, int numElements) {
    Type *constructorElementType = cast<ArrayType>(var->getType()->getElementType())->getElementType();
    Type *newVartype = PointerType::get(ArrayType::get(constructorElementType, numElements), 0);
    var->mutateType(newVartype);
}

void appendGlobalConstructorCall(Module *M, std::string functionName) {
    GlobalVariable *ctors = cast<GlobalVariable>(M->getNamedValue("llvm.global_ctors"));
    int oldNumConstructors = cast<ArrayType>(ctors->getType()->getPointerElementType())->getNumElements();
    outs() << "constructors " << oldNumConstructors << "\n";
    mutateGlobalConsructorNumElements(ctors, oldNumConstructors + 1);

    ConstantArray *initializer = cast<ConstantArray>(ctors->getInitializer());

    Constant **initializers = new Constant *[oldNumConstructors + 1];
    for(int i = 0; i < oldNumConstructors; i++) {
        initializers[i] = initializer->getAggregateElement((unsigned int)i);
    }
    Constant *structValues[3];
    structValues[0] = ConstantInt::get(IntegerType::get(M->getContext(), 32), 1000000);
    structValues[1] = M->getOrInsertFunction(
        functionName,
        Type::getVoidTy(M->getContext()),
        NULL);
    structValues[2] = ConstantPointerNull::get(PointerType::get(IntegerType::get(M->getContext(), 8), 0));
    initializers[oldNumConstructors] = ConstantStruct::getAnon(ArrayRef<Constant *>(&structValues[0], &structValues[3]));
    Constant *newinit = ConstantArray::get(initializer->getType(), ArrayRef<Constant *>(&initializers[0], &initializers[oldNumConstructors + 1]));
    ctors->setInitializer(newinit);
    delete[] initializers;
}

GlobalVariable *addGlobalVariable(Module *M, string name, string value) {
    int N = value.size() + 1;
    LLVMContext &context = M->getContext();
    ArrayType *strtype = ArrayType::get(IntegerType::get(context, 8), N);
    M->getOrInsertGlobal(StringRef(name), strtype);
    ConstantDataArray *constchararray = cast<ConstantDataArray>(ConstantDataArray::get(context, ArrayRef<uint8_t>((uint8_t *)value.c_str(), N)));
    GlobalVariable *str = M->getNamedGlobal(StringRef(name));
    str->setInitializer(constchararray);
    return str;
}

Instruction *addStringInstr(Module *M, string name, string value) {
    // check if already exists first
    GlobalVariable *probe = M->getNamedGlobal(name);
    outs() << "addStringInstr probe=" << probe << "\n";
    if(probe != 0) {
        outs() << "string aleady exists, reusing  " << "\n";
        return addStringInstrExistingGlobal(M, name);
    }

    GlobalVariable *var = addGlobalVariable(M, name, value);

    int N = value.size() + 1;
    LLVMContext &context = M->getContext();
    ArrayType *arrayType = ArrayType::get(IntegerType::get(context, 8), N);
    Value * indices[2];
    indices[0] = ConstantInt::getSigned(IntegerType::get(context, 32), 0);
    indices[1] = ConstantInt::getSigned(IntegerType::get(context, 32), 0);
    GetElementPtrInst *elem = GetElementPtrInst::CreateInBounds(arrayType, var, ArrayRef<Value *>(indices, 2));
    return elem;
}

Instruction *addStringInstrExistingGlobal(Module *M, string name) {
    // GlobalVariable *var = addGlobalVariable(M, name, value);
    GlobalVariable *var = M->getNamedGlobal(name);

    Type *varType = var->getType();
    if(ArrayType *arrayType1 = dyn_cast<ArrayType>(varType->getPointerElementType())) {
        int N = arrayType1->getNumElements();

        LLVMContext &context = M->getContext();
        ArrayType *arrayType = ArrayType::get(IntegerType::get(context, 8), N);
        // Type *varType = var->getType();
        // outs() << "arrayType->dump()" << "\n";
        // ArrayType *elemType = cast<ArrayType>(varType->getPointerElementType());
        // elemType->dump();
        // outs() << "\n";
        // outs() << "numelements " << elemType->getNumElements() << "\n";
        // return 0;
        Value * indices[2];
        indices[0] = ConstantInt::getSigned(IntegerType::get(context, 32), 0);
        indices[1] = ConstantInt::getSigned(IntegerType::get(context, 32), 0);
        GetElementPtrInst *elem = GetElementPtrInst::CreateInBounds(arrayType, var, ArrayRef<Value *>(indices, 2));
        return elem;
    } else {
        throw runtime_error("unexpected type at addStringInstrExistingGlobal, not an arraytype");
    }
}

llvm::Constant *createInt32Constant(llvm::LLVMContext *context, int value) {
    return ConstantInt::getSigned(IntegerType::get(*context, 32), value);
}

void updateAddressSpace(Value *value, int newSpace) {
    Type *elementType = value->getType()->getPointerElementType();
    Type *newType = PointerType::get(elementType, newSpace);
    value->mutateType(newType);
}

void copyAddressSpace(Value *src, Value *dest) {
    // copies address space from src value to dest value
    int srcTypeID = src->getType()->getTypeID();
    if(srcTypeID != Type::PointerTyID) { // not a pointer, so skipe
        return;
    }
    if(PointerType *srcType = dyn_cast<PointerType>(src->getType())) {
        if(isa<PointerType>(dest->getType())) {
            int addressspace = srcType->getAddressSpace();
            if(addressspace != 0) {
                updateAddressSpace(dest, addressspace);
            }
        }
    }
}
