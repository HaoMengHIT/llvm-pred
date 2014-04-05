#include "loop.h"
#include "util.h"
#include "config.h"
#include "debug.h"
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <map>
#include <algorithm>

namespace lle
{
	using namespace llvm;

	//find start value fron induction variable
	static Value* tryFindStart(PHINode* IND,Loop* l,BasicBlock*& StartBB)
	{
		Loop& L = *l;
		if(L->getLoopPredecessor()){ 
			StartBB = L->getLoopPredecessor();
			return IND->getIncomingValueForBlock(StartBB);
		} else {
			Value* start = NULL;
			for(int I = 0,E = IND->getNumIncomingValues();I!=E;++I){
				if(L->contains(IND->getIncomingBlock(I))) continue;
				//if there are many entries, assume they are all equal
				//****??? should do castoff ???******
				if(start && start != IND->getIncomingValue(I)) return NULL;
				start = IND->getIncomingValue(I);
				StartBB = IND->getIncomingBlock(I);
			}
			return start;
		}
	}

	Value* Loop::insertLoopCycle()
	{
		// inspired from Loop::getCanonicalInductionVariable
		BasicBlock *H = self->getHeader();
		BasicBlock* LoopPred = self->getLoopPredecessor();
		BasicBlock* startBB = NULL;//which basicblock stores start value
		int OneStep = 0;// the extra add or plus step for calc

		DEBUG(outs()<<"\n");
		DEBUG(outs()<<*loop);
		DEBUG(outs()<<"loop simplify:"<<self->isLoopSimplifyForm()<<"\n");
		DEBUG(outs()<<"loop  depth:"<<self->getLoopDepth()<<"\n");
		/** whats difference on use of predecessor and preheader??*/
		//RET_ON_FAIL(self->getLoopLatch()&&self->getLoopPreheader());
		//assert(self->getLoopLatch() && self->getLoopPreheader() && "need loop simplify form" );
		RET_ON_FAIL(self->getLoopLatch());
		assert(self->getLoopLatch() && "need loop simplify form" );

		BasicBlock* TE = NULL;//True Exit
		SmallVector<BasicBlock*,4> Exits;
		self->getExitingBlocks(Exits);

		if(Exits.size()==1) TE = Exits.front();
		else{
			if(std::find(Exits.begin(),Exits.end(),self->getLoopLatch())!=Exits.end()) TE = self->getLoopLatch();
			else{
				SmallVector<llvm::Loop::Edge,4> ExitEdges;
				self->getExitEdges(ExitEdges);
				//stl 用法,先把所有满足条件的元素(出口的结束符是不可到达)移动到数组的末尾,再统一删除
				ExitEdges.erase(std::remove_if(ExitEdges.begin(), ExitEdges.end(), 
							[](llvm::Loop::Edge& I){
							return isa<UnreachableInst>(I.second->getTerminator());
							}), ExitEdges.end());
				if(ExitEdges.size()==1) TE = const_cast<BasicBlock*>(ExitEdges.front().first);
			}
		}

		//process true exit
		RET_ON_FAIL(TE);
		assert(TE && "need have a true exit");

		Instruction* IndOrNext = NULL;
		Value* END = NULL;

		if(isa<BranchInst>(TE->getTerminator())){
			const BranchInst* EBR = cast<BranchInst>(TE->getTerminator());
			RET_ON_FAIL(EBR->isConditional());
			assert(EBR->isConditional());
			ICmpInst* EC = dyn_cast<ICmpInst>(EBR->getCondition());
			RET_ON_FAIL(EC->getPredicate() == EC->ICMP_EQ || EC->getPredicate() == EC->ICMP_SGT);
			assert(VERBOSE(EC->getPredicate() == EC->ICMP_EQ || EC->getPredicate() == EC->ICMP_SGT,EC) && "why end condition is not ==");
			if(EC->getPredicate() == EC->ICMP_SGT) OneStep += 1;
			IndOrNext = dyn_cast<Instruction>(castoff(EC->getOperand(0)));
			END = EC->getOperand(1);
			DEBUG(outs()<<"end   value:"<<*EC<<"\n");
		}else if(isa<SwitchInst>(TE->getTerminator())){
			SwitchInst* ESW = const_cast<SwitchInst*>(cast<SwitchInst>(TE->getTerminator()));
			IndOrNext = dyn_cast<Instruction>(castoff(ESW->getCondition()));
			for(auto I = ESW->case_begin(),E = ESW->case_end();I!=E;++I){
				if(!self->contains(I.getCaseSuccessor())){
					RET_ON_FAIL(!END);
					assert(!END && "shouldn't have two ends");
					END = I.getCaseValue();
				}
			}
			DEBUG(outs()<<"end   value:"<<*ESW<<"\n");
		}else{
			RET_ON_FAIL(0);
			assert(0 && "unknow terminator type");
		}

		RET_ON_FAIL(self->isLoopInvariant(END));
		assert(self->isLoopInvariant(END) && "end value should be loop invariant");


		Value* start = NULL;
		Value* ind = NULL;
		Instruction* next = NULL;
		bool addfirst = false;//add before icmp ed

		DISABLE(outs()<<*IndOrNext<<"\n");
		if(isa<LoadInst>(IndOrNext)){
			//memory depend analysis
			Value* PSi = IndOrNext->getOperand(0);//point type Step.i

			int SICount[2] = {0};//store in predecessor count,store in loop body count
			for( auto I = PSi->use_begin(),E = PSi->use_end();I!=E;++I){
				DISABLE(outs()<<**I<<"\n");
				StoreInst* SI = dyn_cast<StoreInst>(*I);
				if(!SI || SI->getOperand(1) != PSi) continue;
				if(!start&&self->isLoopInvariant(SI->getOperand(0))) {
					if(SI->getParent() != LoopPred)
						if(std::find(pred_begin(LoopPred),pred_end(LoopPred),SI->getParent()) == pred_end(LoopPred)) continue;
					start = SI->getOperand(0);
					startBB = SI->getParent();
					++SICount[0];
				}
				Instruction* SI0 = dyn_cast<Instruction>(SI->getOperand(0));
				if(self->contains(SI) && SI0 && SI0->getOpcode() == Instruction::Add){
					next = SI0;
					++SICount[1];
				}

			}
			RET_ON_FAIL(SICount[0]==1 && SICount[1]==1);
			assert(SICount[0]==1 && SICount[1]==1);
			ind = IndOrNext;
		}else{
			if(isa<PHINode>(IndOrNext)){
				PHINode* PHI = cast<PHINode>(IndOrNext);
				ind = IndOrNext;
				if(castoff(PHI->getIncomingValue(0)) == castoff(PHI->getIncomingValue(1)) && PHI->getParent() != H)
					ind = castoff(PHI->getIncomingValue(0));
				addfirst = false;
			}else if(IndOrNext->getOpcode() == Instruction::Add){
				next = IndOrNext;
				addfirst = true;
			}else{
				RET_ON_FAIL(0);
				assert(0 && "unknow how to analysis");
			}

			for(auto I = H->begin();isa<PHINode>(I);++I){
				PHINode* P = cast<PHINode>(I);
				if(ind && P == ind){
					//start = P->getIncomingValueForBlock(self->getLoopPredecessor());
					start = tryFindStart(P, this, startBB);
					next = dyn_cast<Instruction>(P->getIncomingValueForBlock(self->getLoopLatch()));
				}else if(next && P->getIncomingValueForBlock(self->getLoopLatch()) == next){
					//start = P->getIncomingValueForBlock(self->getLoopPredecessor());
					start = tryFindStart(P, this, startBB);
					ind = P;
				}
			}
		}


		RET_ON_FAIL(start);
		assert(start && "couldn't find a start value");
		//process complex loops later
		//DEBUG(if(self->getLoopDepth()>1 || !self->getSubLoops().empty()) return NULL);
		DEBUG(outs()<<"start value:"<<*start<<"\n");
		DEBUG(outs()<<"ind   value:"<<*ind<<"\n");
		DEBUG(outs()<<"next  value:"<<*next<<"\n");


		//process non add later
		unsigned next_phi_idx = 0;
		ConstantInt* Step = NULL,*PrevStep = NULL;/*only used if next is phi node*/
		PHINode* next_phi = dyn_cast<PHINode>(next);
		do{
			if(next_phi) {
				next = dyn_cast<Instruction>(next_phi->getIncomingValue(next_phi_idx));
				RET_ON_FAIL(next);
				DEBUG(outs()<<"next phi "<<next_phi_idx<<":"<<*next<<"\n");
				if(Step&&PrevStep){
					RET_ON_FAIL(Step->getSExtValue() == PrevStep->getSExtValue());
					assert(Step->getSExtValue() == PrevStep->getSExtValue());
				}
				PrevStep = Step;
			}
			RET_ON_FAIL(next->getOpcode() == Instruction::Add);
			assert(next->getOpcode() == Instruction::Add && "why induction increment is not Add");
			RET_ON_FAIL(next->getOperand(0) == ind);
			assert(next->getOperand(0) == ind && "why induction increment is not add it self");
			Step = dyn_cast<ConstantInt>(next->getOperand(1));
			RET_ON_FAIL(Step);
			assert(Step);
		}while(next_phi && ++next_phi_idx<next_phi->getNumIncomingValues());
		//RET_ON_FAIL(Step->equalsInt(1));
		//assert(VERBOSE(Step->equalsInt(1),Step) && "why induction increment number is not 1");


		Value* RES = NULL;
		//if there are no predecessor, we can insert code into start value basicblock
		BasicBlock* insertBB = LoopPred?:startBB;
		IRBuilder<> Builder(insertBB->getTerminator());
		assert(start->getType()->isIntegerTy() && END->getType()->isIntegerTy() && " why increment is not integer type");
		if(start->getType() != END->getType()){
			start = Builder.CreateCast(CastInst::getCastOpcode(start, false,
						END->getType(), false),start,END->getType());
		}
		if(Step->isMinusOne())
			RES = Builder.CreateSub(start,END);
		else//Step Couldn't be zero
			RES = Builder.CreateSub(END, start);
		if(addfirst) OneStep -= 1;
		if(Step->isMinusOne()) OneStep*=-1;
		assert(OneStep<=1 && OneStep>=-1);
		RES = (OneStep==1)?Builder.CreateAdd(RES,Step):(OneStep==-1)?Builder.CreateSub(RES, Step):RES;
		if(!Step->isMinusOne()&&!Step->isOne())
			RES = Builder.CreateSDiv(RES, Step);

		return cycle = RES;
	}

}
