#ifndef LLE_LOOP_H_H
#define LLE_LOOP_H_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instruction.h>

#include <iostream>
#include <map>
#include <stdlib.h>

namespace lle
{
   class NotFound: public std::runtime_error
   {
      size_t line_no;
      public:
      explicit NotFound(size_t src_line, const std::string& what):
         runtime_error(what), line_no(src_line) {}
      explicit NotFound(size_t src_line, llvm::raw_ostream& what):
         runtime_error(static_cast<llvm::raw_string_ostream&>(what).str()), line_no(src_line) {}
      size_t get_line(){ return line_no;}
   };
#define not_found(what) NotFound(__LINE__, what)

	class CompareLTC:public llvm::ModulePass
	{
		//statistics variable:{
		//}
      struct AnalysisedLoop {
         int AdjustStep;
         llvm::Value* Start, *Step, *End, *Ind, *TripCount;
         void* userdata;
      };
		// a stable cache, the index of Loop in df order -> LoopTripCount
      using LoopInfoType = llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>;
      std::vector<AnalysisedLoop> CycleMap;

      std::vector<LoopInfoType*> LoopVec;

      ///////////////////////////////////
      // an unstable cache, the Loop -> index of Loop in df order
      llvm::DenseMap<llvm::Loop*, size_t> LoopMap;
      llvm::DenseMap<llvm::Instruction*, std::vector<llvm::Instruction*> > MpiMap;
      AnalysisedLoop analysis(llvm::Loop*, llvm::Function&);
		public:
		static char ID;
		explicit CompareLTC():ModulePass(ID){ }
		void getAnalysisUsage(llvm::AnalysisUsage&) const;
      void FindDepIns(llvm::Instruction* I, std::vector<llvm::Instruction*>& DepIns) const;
      void FindMPIDepIns(llvm::Instruction* I); 
      bool FindAllDepIns(const AnalysisedLoop* AL, std::vector<llvm::Instruction*>& DepIns) const;
		bool runOnModule(llvm::Module& M);
		void print(llvm::raw_ostream&,const llvm::Module*) const;
      // use this before getTripCount, to make a stable Loop Index Order
      const AnalysisedLoop* get(llvm::Loop* L) const {
         auto ite = LoopMap.find(L);
         if (ite != LoopMap.end()) {
            auto AL = &CycleMap[ite->second];
            return (AL->Start) ? AL : NULL;
         }
         return NULL;
      }
	};
}

#endif
