//
//  ContantBranch.cpp
//  uscc
//
//  Implements Constant Branch Folding opt pass.
//  This converts conditional branches on constants to
//  unconditional branches.
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
#pragma clang diagnostic pop
#include <set>

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool ConstantBranch::runOnFunction(Function& F)
{
	bool changed = false;
	
	// PA5 

	// Make a set that contains the instructions we'll remove
	std::set<BranchInst*> removeSet;

	for (auto& BB : F) {
		for (auto& I : BB) {
			if (BranchInst* br = dyn_cast<BranchInst>(&I)) {
				if (br->isConditional() && isa<ConstantInt>(br->getCondition())) {
					removeSet.insert(br);
					changed = true;
				}
			}
		}
	}

	for (auto& br : removeSet) {
		auto cond = dyn_cast<ConstantInt>(br->getCondition())->getValue().getBoolValue();
		auto brParent = br->getParent();
		auto trueSucc = br->getSuccessor(0);
		auto falseSucc = br->getSuccessor(1);

		if (cond) {
			BranchInst::Create(trueSucc, brParent);
			falseSucc->removePredecessor(brParent);
		}
		else {
			BranchInst::Create(falseSucc, brParent);
			trueSucc->removePredecessor(brParent);
		}
		br->eraseFromParent();
	}
	
	return changed;
}

void ConstantBranch::getAnalysisUsage(AnalysisUsage& Info) const
{
	// PA5
	Info.addRequired<ConstantOps>(); 
}
	
} // opt
} // uscc

char uscc::opt::ConstantBranch::ID = 0;
