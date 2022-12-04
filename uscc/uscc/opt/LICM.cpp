//
//  LICM.cpp
//  uscc
//
//  Implements basic loop invariant code motion
//
//---------------------------------------------------------
//  Copyright (c) 2014, Sanjay Madhav
//  All rights reserved.
//
//  This file is distributed under the BSD license.
//  See LICENSE.TXT for details.
//---------------------------------------------------------
#include "Passes.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/ValueTracking.h>
#pragma clang diagnostic pop

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool LICM::runOnLoop(llvm::Loop *L, llvm::LPPassManager &LPM)
{
	mChanged = false;
	
	// PA5 

	// Save the current loop
	mCurrLoop = L;
	// Grab the loop info
	mLoopInfo = &getAnalysis<LoopInfo>();
	// Grab the dominator tree
	mDomTree = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();

	hoistPreOrder(mDomTree->getNode(mCurrLoop->getHeader()));

	return mChanged;
}

bool LICM::isSafeToHoistInstr(llvm::Instruction* I) {
	// operands are not loop-invariants
	if (!mCurrLoop->hasLoopInvariantOperands(I)) {
		return false;
	}

	// has side effect
	if (!isSafeToSpeculativelyExecute(I)) {
		return false;
	}

	if (!isa<BinaryOperator>(I) && !isa<CastInst>(I) && !isa<SelectInst>(I)
			&& !isa<GetElementPtrInst>(I) && !isa<CmpInst>(I)) {
		return false;
	}	

	return true;
}

void LICM::hoistInstr(llvm::Instruction* I) {
	I->moveBefore(mCurrLoop->getLoopPreheader()->getTerminator());
	mChanged = true;
}

void LICM::hoistPreOrder(llvm::DomTreeNode* domNode) {
	auto BB = domNode->getBlock();

	if (mLoopInfo->getLoopFor(BB) == mCurrLoop) {
		auto iter = BB->begin();
		while (iter != BB->end()) {
			auto I = iter;
			iter ++;
			if (isSafeToHoistInstr(I)) {
				hoistInstr(I);
			}
		}
	}

	for (auto& child : domNode->getChildren()) {
		hoistPreOrder(child);
	}
}

void LICM::getAnalysisUsage(AnalysisUsage &Info) const
{
	// PA5

	// LICM does not modify the CFG
	Info.setPreservesCFG();
	// Execute after dead blocks have been removed
	Info.addRequired<DeadBlocks>();
	// Use the built-in Dominator tree and loop info passes
	Info.addRequired<DominatorTreeWrapperPass>();
	Info.addRequired<LoopInfo>(); 
}
	
} // opt
} // uscc

char uscc::opt::LICM::ID = 0;
