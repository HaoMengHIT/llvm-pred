#include "ddg.h"
#include "util.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/ADT/PostOrderIterator.h>

#include "debug.h"

using namespace std;
using namespace lle;
using namespace llvm;

#define LHS(N) N->impl().front()->second
#define RHS(N) N->impl().back()->second

/** TODO LHS and RHS doesn't consider content match. 所以可能错位 */

static void to_expr(Value* V, DDGNode* N, int& ref_num);

Twine& DDGNode::expr(int prio)
{
   if(prio<this->prio) return bk;
   return root;
}

void DDGNode::set_expr(Twine lhs, Twine rhs, int prio)
{
   this->lhs = lhs;
   this->rhs = rhs;
   this->root=this->lhs+this->rhs;
   this->lbk="("+this->root;
   this->bk=lbk+")";
   this->prio = prio;
}

void DDGNode::ref(int R)
{
   // a small trick: use bk to store reference expr.
   // while root store original expr.
   // and a very low prio means always return bracket expr.
   // so DDGraph can read real expr from root directly.
   ref_num_ = R;
   prio = 16;
   lbk=Twine(ref_num_);
   bk="#"+lbk;
}

DDGValue DDGraph::make_value(Value *root, DDGNode::Flags flags)
{
   DDGNode n;
   n.flags_ = flags;
   n.load_tg_ = nullptr;
   n.ref_num_ = 0;
   n.ref_count = 0;
   n.root="";
   return make_pair(root,n);
}

DDGraph::DDGraph(ResolveResult& RR,Value* root) 
{
   auto& r = get<0>(RR);
   auto& u = get<1>(RR);
   auto& c = get<2>(RR);
   for(auto I : r){
      this->insert(make_value(I,DDGNode::NORMAL));
   }
   for(auto I : u){
      this->insert(make_value(I,DDGNode::UNSOLVED));
   }
   for(auto& N : *this){
      auto found = c.find(N.first);
      DDGNode& node = N.second;
      if(node.flags() & DDGNode::UNSOLVED) continue;
      Use* implicity = (found == c.end())?nullptr:found->second;
      if(implicity){
         DDGValue& v = *this->find(implicity->getUser());
         DDGNode& to = v.second;
         node.impl().push_back(&v);
         ++v.second.ref_count;
         node.flags_ = DDGNode::IMPLICITY;
         if(isa<CallInst>(implicity->getUser())){
            Argument* arg = findCallInstArgument(implicity);
            auto found = c.find(cast<Value>(arg));
            Use* link = (found==c.end())?nullptr:found->second;
            if(link){
               node.load_tg_ = &*this->find(link->getUser());
               //++node.load_tg_->second.ref_count; // shouldn't add ref count for load_tg
               to.impl().push_back(node.load_inst());
               to.flags_ = DDGNode::IMPLICITY;
            }
         }
      }else{
         Instruction* Inst = dyn_cast<llvm::Instruction>(N.first);
         if(!Inst) continue;
         for(auto O = Inst->op_begin(),E=Inst->op_end();O!=E;++O){
            auto Target = this->find(*O);
            if(Target != this->end()){
               auto v = &*this->find(*O);
               ++v->second.ref_count;
               node.impl().push_back(v);
            }
         }
      }
   }
   this->root = &*this->find(root);
}

static void to_expr(LoadInst* LI, DDGNode* N, int& R)
{
   if(N->flags() == DDGNode::UNSOLVED){
      raw_string_ostream O(N->expr_buf);
      pretty_print(LI, O, false); // FIXME use pretty_print to get loaded value's name, not stable.
      N->set_expr(O.str(), "");
   }else{
      Assert(N->impl().size()==1,"");
      if(isa<CallInst>(N->impl().front()->first)){
         N->set_expr(LHS(N).expr()+"{", N->load_inst()->second.expr()+"}");
      }else{
         N->set_expr(LHS(N).expr(), "");
      }
   }
}

static void to_expr(Constant* C, DDGNode* N, int& R)
{
   if(isa<GlobalValue>(C))
      N->set_expr("@", C->getName());
   else{
      raw_string_ostream O(N->expr_buf);
      pretty_print(C,O);
      N->set_expr(N->expr_buf, "");
   }
}

static void to_expr(PHINode* PHI, DDGNode* N, int& R)
{
   auto& node1 = LHS(N);
   // if a node is empty. means there is a self reference.
   if(node1.expr().isTriviallyEmpty()) N->expr_buf = PHINODE_CIRCLE;
   else N->expr_buf = node1.expr(6).str();
   for(auto I = N->impl().begin()+1, E = N->impl().end(); I!=E; ++I){
      auto& node = (*I)->second;
      if(node.expr().isTriviallyEmpty()) N->expr_buf += "||" PHINODE_CIRCLE;
      else N->expr_buf += ("||"+(*I)->second.expr(6)).str();
   }
   N->set_expr(N->expr_buf,"",14);
}

static void to_expr(Value* V, DDGNode* N, int& ref_num)
{
   if(Constant* C = dyn_cast<Constant>(V)){
      to_expr(C,N,ref_num);
      return;
   } else if(isa<Argument>(V)){
      N->set_expr("%", V->getName());
      return;
   }

   Instruction* I = dyn_cast<Instruction>(V);
   Assert(I,*I);

   if(auto BI = dyn_cast<BinaryOperator>(V)){
      Assert(N->impl().size()==2,"");
      auto Sym = lookup_sym(BI);
      int P = Sym.second;
      N->set_expr(LHS(N).expr(P),Sym.first+RHS(N).expr(P), P);
   }else if(auto CI = dyn_cast<CmpInst>(V)){
      Assert(N->impl().size()==2,"");
      auto Sym = lookup_sym(CI);
      int P = Sym.second;
      N->set_expr(LHS(N).expr(P)+Sym.first,RHS(N).expr(P), P);
   }else if(isa<StoreInst>(V)){
      Assert(N->impl().size()==1,"");
      N->set_expr(LHS(N).expr(), "");
   }else if(auto CI = dyn_cast<CastInst>(V)){
      raw_string_ostream SS(N->expr_buf);
      CI->getDestTy()->print(SS);
      SS<<"{";
      N->set_expr(SS.str(), LHS(N).expr(14)+"}",0);
   }else if(isa<SelectInst>(V)){
      Assert(N->impl().size()==3,"");
      raw_string_ostream SS(N->expr_buf);
      SS<<"("<<LHS(N).expr()<<")?";
      N->set_expr(SS.str()+N->impl()[1]->second.expr(),":"+RHS(N).expr());
   }else if(isa<ExtractElementInst>(V)){
      Assert(N->impl().size()==2,"");
      N->set_expr(LHS(N).expr()+"[", RHS(N).expr(14)+"]", 0);
   }
   else if(isa<AllocaInst>(V))
      N->set_expr("%", V->getName());
   else if(auto LI = dyn_cast<LoadInst>(V))
      to_expr(LI, N, ref_num);
   else if(auto CI = dyn_cast<CallInst>(V)){
      N->expr_buf = CI->getCalledFunction()->getName();
      N->set_expr(N->expr_buf, "");
   }else if(auto PHI = dyn_cast<PHINode>(V))
      to_expr(PHI, N, ref_num);
   else if(isa<ShuffleVectorInst>(V))
      N->set_expr("too ", "complex");
   else
      Assert(0,*I);
}

Twine DDGraph::expr()
{
   int ref_num = 0;
   vector<DDGNode*> refs;
   for(auto I = po_begin(this), E = po_end(this); I!=E; ++I){
      to_expr(I->first,&I->second,ref_num);
#ifdef EXPR_ENABLE_REF
      if(I->second.ref_count>2 && !isa<Constant>(I->first)){
         I->second.ref(++ref_num);
         refs.push_back(&I->second);
      }
#endif
   }
   if(!refs.empty()){
      root->second.expr_buf = (root->second.expr()+" aka {").str();
      for(auto I : refs)
         root->second.expr_buf += (I->expr()+":"+I->root+"; ").str();
      root->second.expr_buf += "}";
      root->second.root = root->second.expr_buf;
   }
   return root->second.expr();
}