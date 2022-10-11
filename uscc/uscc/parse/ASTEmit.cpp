//
//  ASTNodes.cpp
//  uscc
//
//  Implements the emitIR function for every AST node.
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------

#include "ASTNodes.h"
#include "Emitter.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#pragma clang diagnostic pop

#include <vector>

using namespace uscc::parse;
using namespace llvm;

#define AST_EMIT(a) llvm::Value* a::emitIR(CodeContext& ctx) noexcept

// Program/Functions
AST_EMIT(ASTProgram)
{
	ctx.mModule = new Module("main", ctx.mGlobal);
	
	// Write the global string table
	ctx.mStrings.emitIR(ctx);
	
	// Emit declaration for stdlib "printf", if we need it
	if (ctx.mPrintfIdent != nullptr)
	{
		std::vector<llvm::Type*> printfArgs;
		printfArgs.push_back(llvm::Type::getInt8PtrTy(ctx.mGlobal));
		
		FunctionType* printfType = FunctionType::get(llvm::Type::getInt32Ty(ctx.mGlobal),
													 printfArgs, true);
		
		Function* func = Function::Create(printfType, GlobalValue::LinkageTypes::ExternalLinkage,
										  "printf", ctx.mModule);
		func->setCallingConv(CallingConv::C);
		
		// Map the printf ident to this function
		ctx.mPrintfIdent->setAddress(func);
	}
	
	// Emit code for all the functions
	for (auto f : mFuncs)
	{
		f->emitIR(ctx);
	}
	// A program actually doesn't have a value to return, since everything
	// is stored in Module
	return nullptr;
}

AST_EMIT(ASTFunction)
{
	FunctionType* funcType = nullptr;
	
	// First get the return type (there's only three choices)
	llvm::Type* retType = nullptr;
	if (mReturnType == Type::Int)
	{
		retType = llvm::Type::getInt32Ty(ctx.mGlobal);
	}
	else if (mReturnType == Type::Char)
	{
		retType = llvm::Type::getInt8Ty(ctx.mGlobal);
	}
	else
	{
		retType = llvm::Type::getVoidTy(ctx.mGlobal);
	}
	
	if (mArgs.size() == 0)
	{
		funcType = FunctionType::get(retType, false);
	}
	else
	{

		std::vector<llvm::Type*> args;
		for (auto arg : mArgs)
		{
			args.push_back(arg->getIdent().llvmType());
		}
		
		funcType = FunctionType::get(retType, args, false);
	}
	
	// Create the function, and make it the current one
	ctx.mFunc = Function::Create(funcType,
								 GlobalValue::LinkageTypes::ExternalLinkage,
								 mIdent.getName(), ctx.mModule);
	
	// Now that we have a new function, reset our SSA builder
	ctx.mSSA.reset();
	
	// Map the ident to this function
	mIdent.setAddress(ctx.mFunc);
	
	// Create the entry basic block
	ctx.mBlock = BasicBlock::Create(ctx.mGlobal, "entry", ctx.mFunc);
	// Add and seal this block
	ctx.mSSA.addBlock(ctx.mBlock, true);
	
	// If we have arguments, we need to set the name/value of them
	if (mArgs.size() > 0)
	{
		Function::arg_iterator iter = ctx.mFunc->arg_begin();
		Function::arg_iterator end = ctx.mFunc->arg_end();
		int i = 0;
		while (iter != end)
		{
			Identifier& argIdent = mArgs[i]->getIdent();
			iter->setName(argIdent.getName());
			
			// PA4: Remove the setAddress call
			// (Technically, iter actually has the value of the
			// arg, not its address...but we will use the address
			// member for this value)
			argIdent.setAddress(iter);
			
			// PA4: Write to this identifier
			// argIdent.writeTo(ctx, iter);
			
			++i;
			++iter;
		}
	}
	
	ctx.mFunc->setCallingConv(CallingConv::C);
	
	// Add all the declarations for variables created in this function
	mScopeTable.emitIR(ctx);
	
	// Now emit the body
	mBody->emitIR(ctx);
	
	return ctx.mFunc;
}

AST_EMIT(ASTArgDecl)
{
	// This node actually doesn't have anything to emit
	return nullptr;
}

AST_EMIT(ASTArraySub)
{
	// Evaluate the sub expression to get the desired index
	Value* arrayIdx = mExpr->emitIR(ctx);
	
	// This address should already be saved
	Value* addr = mIdent.readFrom(ctx);
	
	// GEP from the array address
	IRBuilder<> build(ctx.mBlock);
	return build.CreateInBoundsGEP(addr, arrayIdx);
}

// Expressions

AST_EMIT(ASTBadExpr)
{
	// This node will never be emitted
	return nullptr;
}

AST_EMIT(ASTLogicalAnd)
{
	// This is extremely similar to logical or
	
	// Create the block for the RHS
	BasicBlock* rhsBlock = BasicBlock::Create(ctx.mGlobal, "and.rhs", ctx.mFunc);
	// Add the rhs block to SSA (not sealed)
	ctx.mSSA.addBlock(rhsBlock);
	
	// In both "true" and "false" condition, we'll jump to and.end
	// This is because we'll insert a phi node that assume false
	// if the and.end jump was from the lhs block
	BasicBlock* endBlock = BasicBlock::Create(ctx.mGlobal, "and.end", ctx.mFunc);
	// Also not sealed
	ctx.mSSA.addBlock(endBlock);
	
	// Now generate the LHS
	Value* lhsVal = mLHS->emitIR(ctx);
	
	BasicBlock* lhsBlock = ctx.mBlock;
	
	// Add the branch to the end of the LHS
	{
		IRBuilder<> build(ctx.mBlock);
		// We can assume it WILL be an i32 here
		// since it'd have been zero-extended otherwise
		lhsVal = build.CreateICmpNE(lhsVal, ctx.mZero, "tobool");
		build.CreateCondBr(lhsVal, rhsBlock, endBlock);
	}
	
	// rhsBlock should now be sealed
	ctx.mSSA.sealBlock(rhsBlock);
	
	// Code should now be generated in the RHS block
	ctx.mBlock = rhsBlock;
	Value* rhsVal = mRHS->emitIR(ctx);
	
	// This is the final RHS block (for the phi node)
	rhsBlock = ctx.mBlock;
	
	// Add the branch and the end of the RHS
	{
		IRBuilder<> build(ctx.mBlock);
		rhsVal = build.CreateICmpNE(rhsVal, ctx.mZero, "tobool");
		
		// We do an unconditional branch because the phi mode will handle
		// the correct value
		build.CreateBr(endBlock);
	}
	
	// endBlock should now be sealed
	ctx.mSSA.sealBlock(endBlock);
	
	ctx.mBlock = endBlock;
	
	IRBuilder<> build(ctx.mBlock);
	
	// Figure out the value to zext
	Value* zextVal = nullptr;
	
	// If rhs is not also false, we need to make a phi
	if (rhsVal != ConstantInt::getFalse(ctx.mGlobal))
	{
		PHINode* phi = build.CreatePHI(llvm::Type::getInt1Ty(ctx.mGlobal), 2);
		// If we came from the lhs, it had to be false
		phi->addIncoming(ConstantInt::getFalse(ctx.mGlobal), lhsBlock);
		phi->addIncoming(rhsVal, rhsBlock);
		zextVal = phi;
	}
	else
	{
		zextVal = ConstantInt::getFalse(ctx.mGlobal);
	}
	
	return build.CreateZExt(zextVal, llvm::Type::getInt32Ty(ctx.mGlobal));
}

AST_EMIT(ASTLogicalOr)
{
	// Create the block for the RHS
	BasicBlock* rhsBlock = BasicBlock::Create(ctx.mGlobal, "lor.rhs", ctx.mFunc);
	// Add the rhs block to SSA (not sealed)
	ctx.mSSA.addBlock(rhsBlock);

	// In both "true" and "false" condition, we'll jump to lor.end
	// This is because we'll insert a phi node that assume true
	// if the lor.end jump was from the lhs block
	BasicBlock* endBlock = BasicBlock::Create(ctx.mGlobal, "lor.end", ctx.mFunc);
	// Also not sealed
	ctx.mSSA.addBlock(endBlock);
	
	// Now generate the LHS
	Value* lhsVal = mLHS->emitIR(ctx);
	
	BasicBlock* lhsBlock = ctx.mBlock;
	
	// Add the branch to the end of the LHS
	{
		IRBuilder<> build(ctx.mBlock);
		// We can assume it WILL be an i32 here
		// since it'd have been zero-extended otherwise
		lhsVal = build.CreateICmpNE(lhsVal, ctx.mZero, "tobool");
		build.CreateCondBr(lhsVal, endBlock, rhsBlock);
	}
	
	// rhsBlock should now be sealed
	ctx.mSSA.sealBlock(rhsBlock);
	
	// Code should now be generated in the RHS block
	ctx.mBlock = rhsBlock;
	Value* rhsVal = mRHS->emitIR(ctx);
	
	// This is the final RHS block (for the phi node)
	rhsBlock = ctx.mBlock;
	
	// Add the branch and the end of the RHS
	{
		IRBuilder<> build(ctx.mBlock);
		rhsVal = build.CreateICmpNE(rhsVal, ctx.mZero, "tobool");
		
		// We do an unconditional branch because the phi mode will handle
		// the correct value
		build.CreateBr(endBlock);
	}
	
	// endBlock should now be sealed
	ctx.mSSA.sealBlock(endBlock);
	
	ctx.mBlock = endBlock;
	
	IRBuilder<> build(ctx.mBlock);
	
	// Figure out the value to zext
	Value* zextVal = nullptr;
	
	// If rhs is not also true, we need to make a phi
	if (rhsVal != ConstantInt::getTrue(ctx.mGlobal))
	{
		PHINode* phi = build.CreatePHI(llvm::Type::getInt1Ty(ctx.mGlobal), 2);
		// If we came from the lhs, it had to be false
		phi->addIncoming(ConstantInt::getTrue(ctx.mGlobal), lhsBlock);
		phi->addIncoming(rhsVal, rhsBlock);
		zextVal = phi;
	}
	else
	{
		zextVal = ConstantInt::getTrue(ctx.mGlobal);
	}
	
	return build.CreateZExt(zextVal, llvm::Type::getInt32Ty(ctx.mGlobal));
}

AST_EMIT(ASTBinaryCmpOp)
{
	Value* retVal = nullptr;
	
	// PA3 
	IRBuilder<> builder(ctx.mBlock);

	Value* lhs = mLHS->emitIR(ctx);
	Value* rhs = mRHS->emitIR(ctx);

	switch (mOp) {
		case scan::Token::EqualTo:
			retVal = builder.CreateICmpEQ(lhs, rhs, "eq");
			break;
		case scan::Token::NotEqual:
			retVal = builder.CreateICmpNE(lhs, rhs, "ne");
			break;
		case scan::Token::LessThan:
			retVal = builder.CreateICmpSLT(lhs, rhs, "lt");
			break;
		case scan::Token::GreaterThan:
			retVal = builder.CreateICmpSGT(lhs, rhs, "gt");
			break;
	}

	return builder.CreateZExt(retVal, llvm::Type::getInt32Ty(ctx.mGlobal));
}

AST_EMIT(ASTBinaryMathOp)
{
	Value* retVal = nullptr;
	
	// PA3 
	IRBuilder<> builder(ctx.mBlock);

	Value* lhs = mLHS->emitIR(ctx);
	Value* rhs = mRHS->emitIR(ctx);

	switch (mOp) {
		case scan::Token::Plus:
			retVal = builder.CreateAdd(lhs, rhs, "add");
			break;
		case scan::Token::Minus:
			retVal = builder.CreateSub(lhs, rhs, "minus");
			break;
		case scan::Token::Mult:
			retVal = builder.CreateMul(lhs, rhs, "mult");
			break;
		case scan::Token::Div:
			retVal = builder.CreateSDiv(lhs, rhs, "div");
			break;
		case scan::Token::Mod:
			retVal = builder.CreateSRem(lhs, rhs, "mod");
			break;
	}
	
	return retVal;
}

// Value -->
AST_EMIT(ASTNotExpr)
{
	Value* retVal = nullptr;
	
	// PA3 
	IRBuilder<> builder(ctx.mBlock);
	Value* v = mExpr->emitIR(ctx);
	if (mType == Type::Char) {
		retVal = builder.CreateICmpEQ(v, builder.getInt8(0));
	}
	else if (mType == Type::Int) {
		retVal = builder.CreateICmpEQ(v, builder.getInt32(0));
	}
	retVal = builder.CreateZExt(retVal, llvm::Type::getInt32Ty(ctx.mGlobal));

	return retVal;
}

// Factor -->
AST_EMIT(ASTConstantExpr)
{
	Value* retVal = nullptr;
	
	// PA3 

	if (mType == Type::Int) {
		retVal = ConstantInt::get(llvm::Type::getInt32Ty(ctx.mGlobal), mValue);
	}
	else if (mType == Type::Char) {
		retVal = ConstantInt::get(llvm::Type::getInt8Ty(ctx.mGlobal), mValue);
	}
	
	return retVal;
}

AST_EMIT(ASTStringExpr)
{
	return mString->getValue();
}

AST_EMIT(ASTIdentExpr)
{
	return mIdent.readFrom(ctx);
}

AST_EMIT(ASTArrayExpr)
{
	// Generate the array subscript, which'll give us the address
	Value* addr = mArray->emitIR(ctx);

	IRBuilder<> build(ctx.mBlock);
	// Now load this value and return
	
	// NOTE: This still needs to be a load because arrays are in memory
	return build.CreateLoad(addr);
}

AST_EMIT(ASTFuncExpr)
{
	
	// At this point, we can assume the argument types match
	// Create the list of arguments
	std::vector<Value*> callList;
	for (auto arg : mArgs)
	{
		Value* argValue = arg->emitIR(ctx);
		// If this is an array or ptr, we need to change this to a getelemptr
		// (Provided it already isn't one)
		if (!isa<GetElementPtrInst>(argValue) &&
			argValue->getType()->isPointerTy())
		{
			if (argValue->getType()->getPointerElementType()->isArrayTy())
			{
				IRBuilder<> build(ctx.mBlock);
				std::vector<llvm::Value*> gepIdx;
				gepIdx.push_back(ctx.mZero);
				gepIdx.push_back(ctx.mZero);
				
				argValue = build.CreateInBoundsGEP(argValue, gepIdx);
			}
			else
			{
				IRBuilder<> build(ctx.mBlock);				
				// Need to return the address of the specific index in question
				// So need a GEP
				argValue = build.CreateInBoundsGEP(argValue, ctx.mZero);
			}
		}
			
		callList.push_back(argValue);
	}
	
	// Now call the function, and return it
	Value* retVal = nullptr;
	
	IRBuilder<> build(ctx.mBlock);
	if (mType != Type::Void)
	{
		retVal = build.CreateCall(mIdent.getAddress(), callList, "call");
	}
	else
	{
		retVal = build.CreateCall(mIdent.getAddress(), callList);
	}
	
	return retVal;
}

AST_EMIT(ASTIncExpr)
{
	Value* retVal = nullptr;
	
	// PA3 
	IRBuilder<> builder(ctx.mBlock);
	Value* val = mIdent.readFrom(ctx);
	if (mType == Type::Char) {
		retVal = builder.CreateAdd(val, builder.getInt8(1));
	}
	else if (mType == Type::Int) {
		retVal = builder.CreateAdd(val, builder.getInt32(1));
	}
	mIdent.writeTo(ctx, retVal);
	
	return retVal;
}

AST_EMIT(ASTDecExpr)
{
	Value* retVal = nullptr;
	
	// PA3 
	IRBuilder<> builder(ctx.mBlock);
	Value* val = mIdent.readFrom(ctx);
	if (mType == Type::Char) {
		retVal = builder.CreateSub(val, builder.getInt8(1));
	}
	else if (mType == Type::Int) {
		retVal = builder.CreateSub(val, builder.getInt32(1));
	}
	mIdent.writeTo(ctx, retVal);

	
	return retVal;
}

AST_EMIT(ASTAddrOfArray)
{
	return mArray->emitIR(ctx);
}

AST_EMIT(ASTToIntExpr)
{
	Value* exprVal = mExpr->emitIR(ctx);
	IRBuilder<> build(ctx.mBlock);
	return build.CreateSExt(exprVal, llvm::Type::getInt32Ty(ctx.mGlobal), "conv");
}

AST_EMIT(ASTToCharExpr)
{
	Value* exprVal = mExpr->emitIR(ctx);
	IRBuilder<> build(ctx.mBlock);
	return build.CreateTrunc(exprVal, llvm::Type::getInt8Ty(ctx.mGlobal), "conv");
}

// Declaration
AST_EMIT(ASTDecl)
{
	// If there's an expression, emit this also and store it in the ident
	if (mExpr)
	{
		Value* declExpr = mExpr->emitIR(ctx);
		
		IRBuilder<> build(ctx.mBlock);
		// If this is a string, we have to memcpy
		if (declExpr->getType()->isPointerTy())
		{
			// This address should already be saved
			Value* arrayLoc = mIdent.readFrom(ctx);
			
			// GEP the address of the src
			std::vector<llvm::Value*> gepIdx;
			gepIdx.push_back(ctx.mZero);
			gepIdx.push_back(ctx.mZero);
			
			Value*  src = build.CreateGEP(declExpr, gepIdx);
			
			// Memcpy into the array
			// memcpy(dest, src, size, align, volatile)
			build.CreateMemCpy(arrayLoc, src, mIdent.getArrayCount(), 1);
		}
		else
		{
			// Basic types can just be written
			mIdent.writeTo(ctx, declExpr);
		}
	}
	
	return nullptr;
}

// Statements
AST_EMIT(ASTCompoundStmt)
{
	// PA3 

	// decl
	for (auto& decl : mDecls) {
		decl->emitIR(ctx);
	}

	// stmt
	for (auto& stmt : mStmts) {
		stmt->emitIR(ctx);
	}
	
	return nullptr;
}

AST_EMIT(ASTAssignStmt)
{
	// PA3 
	mIdent.writeTo(ctx, mExpr->emitIR(ctx));
	
	return nullptr;
}

AST_EMIT(ASTAssignArrayStmt)
{
	// Generate the expression
	Value* exprVal = mExpr->emitIR(ctx);
	
	// Generate the array subscript, which'll give us the address
	Value* addr = mArray->emitIR(ctx);

	IRBuilder<> build(ctx.mBlock);
	
	// NOTE: This is still a create store because arrays are always stack-allocated
	build.CreateStore(exprVal, addr);
	
	return nullptr;
}

AST_EMIT(ASTIfStmt)
{
	// PA3 

	// create blocks
	auto thenBlock = BasicBlock::Create(ctx.mGlobal, "if.then", ctx.mFunc);
	BasicBlock* elseBlock;
	if (mElseStmt) {
		elseBlock = BasicBlock::Create(ctx.mGlobal, "if.else", ctx.mFunc);
	}
	auto endBlock = BasicBlock::Create(ctx.mGlobal, "if.end", ctx.mFunc);

	// append condition IR to current block
	auto condVal = mExpr->emitIR(ctx);
	IRBuilder<> builder(ctx.mBlock); // the mBlocks can change because of previous emitIR()
	auto condBool = builder.CreateICmpNE(condVal, ctx.mZero);
	if (mElseStmt) {
		builder.CreateCondBr(condBool, thenBlock, elseBlock);
	}
	else {
		builder.CreateCondBr(condBool, thenBlock, endBlock);
	}

	// then block
	ctx.mBlock = thenBlock;
	mThenStmt->emitIR(ctx);
	builder.SetInsertPoint(ctx.mBlock);
	builder.CreateBr(endBlock);

	// else block
	if (mElseStmt) {
		ctx.mBlock = elseBlock;
		mElseStmt->emitIR(ctx);
		builder.SetInsertPoint(ctx.mBlock);
		builder.CreateBr(endBlock);
	}

	ctx.mBlock = endBlock;
	
	return nullptr;
}

AST_EMIT(ASTWhileStmt)
{
	// PA3 

	// create blocks
	auto condBlock = BasicBlock::Create(ctx.mGlobal, "while.cond", ctx.mFunc);
	auto bodyBlock = BasicBlock::Create(ctx.mGlobal, "while.body", ctx.mFunc);
	auto endBlock = BasicBlock::Create(ctx.mGlobal, "while.end", ctx.mFunc);

	// unconditional branch to cond block
	IRBuilder<> builder(ctx.mBlock);
	builder.CreateBr(condBlock);

	// emit IR for cond block
	ctx.mBlock = condBlock;
	auto condVal = mExpr->emitIR(ctx);

	// conditonal branch from cond block
	builder.SetInsertPoint(ctx.mBlock);
	auto condBool = builder.CreateICmpNE(condVal, ctx.mZero);
	builder.CreateCondBr(condBool, bodyBlock, endBlock);

	// emit IR for body block
	ctx.mBlock = bodyBlock;
	mLoopStmt->emitIR(ctx);
	builder.SetInsertPoint(ctx.mBlock);
	builder.CreateBr(condBlock);

	// emit IR for end block
	ctx.mBlock = endBlock;
	
	return nullptr;
}

AST_EMIT(ASTReturnStmt)
{
	// PA3 
	IRBuilder<> builder(ctx.mBlock);

	// expr being returned	
	if (mExpr) {
		builder.CreateRet(mExpr->emitIR(ctx));
	}
	// void
	else {
		builder.CreateRetVoid();
	}

	return nullptr;
}

AST_EMIT(ASTExprStmt)
{
	// PA3 
	mExpr->emitIR(ctx);

	return nullptr;
}

AST_EMIT(ASTNullStmt)
{
	// Doesn't do anything (hence empty)
	return nullptr;
}
