//
//  DeadBlocks.cpp
//  uscc
//
//  Implements Dead Block Removal optimization pass.
//  This removes blocks from the CFG which are unreachable.
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
#include <llvm/IR/CFG.h>
#include <llvm/ADT/DepthFirstIterator.h>
#pragma clang diagnostic pop
#include <set>

using namespace llvm;

namespace uscc
{
namespace opt
{
	
bool DeadBlocks::runOnFunction(Function& F)
{
	bool changed = false;
	
	// PA5
	std::set<BasicBlock*> visitedBlock;
	auto begin_iter = df_ext_begin(&F, visitedBlock);
	auto end_iter = df_ext_end(&F, visitedBlock);
	while (begin_iter != end_iter) {
		begin_iter ++;
	}

	std::set<BasicBlock*> unreachableBlock;
	for (auto& BB : F) {
		if (visitedBlock.find(&BB) == visitedBlock.end()) {
			unreachableBlock.insert(&BB);
			changed = true;
		}
	}

	for (auto BB : unreachableBlock) {
		auto begin_iter = succ_begin(BB);
		auto end_iter = succ_end(BB);
		while (begin_iter != end_iter) {
			begin_iter->removePredecessor(BB);
			begin_iter ++;
		}
		BB->eraseFromParent();
	}
	
	return changed;
}
	
void DeadBlocks::getAnalysisUsage(AnalysisUsage& Info) const
{
	// PA5
	Info.addRequired<ConstantBranch>(); 
}

} // opt
} // uscc

char uscc::opt::DeadBlocks::ID = 0;
