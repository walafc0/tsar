
#ifndef _program_hpp_
#define _program_hpp_

#include <iostream>
#include <sstream>
#include <stdbool.h>

#include "config.h"
#include "TestThread.hpp"


using namespace std;

class Program {

   int nb_threads;
   int nb_max_incr;
   int nb_locks;
   int nb_vars;
   int line_size;
   bool lock_in_line, vars_in_line;
   int tab_size;
   TestThread ** threads;

   public:

   Program(int nb_procs, int nb_locks, int nb_max_accesses, int line_size, bool lock_in_line, bool vars_in_line){
      this->nb_threads = nb_procs;
      this->nb_max_incr = nb_max_accesses;
      this->nb_locks = nb_locks;
      this->nb_vars = nb_locks;
      this->line_size = line_size;
      this->lock_in_line = lock_in_line;
      this->vars_in_line = vars_in_line;
      this->tab_size = (lock_in_line ? ceil((float) nb_locks / line_size) * line_size : nb_locks * line_size) + (vars_in_line ? ceil((float) nb_vars / line_size) * line_size : nb_vars * line_size);
      this->threads = new TestThread * [nb_threads];

      for (int i = 0; i < nb_threads; i++){
         threads[i] = new TestThread(nb_locks, nb_max_accesses, line_size, lock_in_line, vars_in_line, i);
      }
   }

   ~Program(){
      for (int i = 0; i < nb_threads; i++){
         delete threads[i];
      }
      delete [] threads;
   }


   string write_main_task() {
      stringstream res;
      res << "#include \"srl.h\"" << endl;
      res << "#include \"stdio.h\"" << endl;
      res << "#include \"test_llsc_main_proto.h\"" << endl;
      res << "#include \"functions.h\"" << endl;
      res << endl;
      res << "/*" << endl;
      res << " * Test generated for a dsx_vm compilation" << endl;
      res << " * NB_LOCKS : " << nb_locks << endl;
      res << " * NB_VARS : " << nb_vars << endl;
      if (lock_in_line) {
         res << " * LOCKS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * LOCKS PLACEMENT : vertical" << endl;
      }
      if (vars_in_line) {
         res << " * VARS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * VARS PLACEMENT : vertical" << endl;
      }
      res << " * NB_MAX_INCRS : " << nb_max_incr << endl;
      res << " * LINE_SIZE : " << LINE_SIZE << endl;
      res << " */" << endl;
      res << endl;
      res << "int * tab;" << endl;
      res << endl;
      res << threads[0]->write_task();
      res << endl;
      res << endl;
      res << "FUNC(test_llsc_main_func) {" << endl;
      res << endl;
      res << "   int i;" << endl;
      res << endl;
      res << "   srl_memspace_t memspace = SRL_GET_MEMSPACE(table);" << endl;
      res << "   tab = (int *) SRL_MEMSPACE_ADDR(memspace);" << endl;
      res << "   srl_barrier_t barrier = SRL_GET_BARRIER(barrier);" << endl;
      res << "   srl_barrier_t barrier2 = SRL_GET_BARRIER(barrier2);" << endl;
      res << endl;
      res << "   // Initialisation du tableau" << endl;
      res << "   for (i = 0; i < " << tab_size << "; i++) {" << endl;
      res << "      tab[i] = 0;" << endl;
      res << "   }" << endl;
      res << endl;
      res << "   barrier_wait(barrier);" << endl;
      res << endl;
      res << "   run0();" << endl;
      res << endl;
      res << "   barrier_wait(barrier2);" << endl;
      res << endl;
      res << "   for (i = 0; i < " << tab_size << "; i++) {" << endl;
      res << "      if (tab[i] != 0) {" << endl;
      res << "         giet_tty_printf(\"tab[%d] final : %d\\n\", i, tab[i]);" << endl;
      res << "      }" << endl;
      res << "   }" << endl;
      res << endl;
      res << "   giet_raise_sigint();" << endl;
      res << endl;
      res << "   srl_exit();" << endl;
      res << "}" << endl;
      res << endl;

      return res.str();
   }

   
   string write_task_no_tty() {
      stringstream res;
      res << "#include \"srl.h\"" << endl;
      res << "#include \"stdio.h\"" << endl;
      res << "#include \"test_llsc_no_tty_proto.h\"" << endl;
      res << "#include \"functions.h\"" << endl;
      res << endl;
      res << "/*" << endl;
      res << " * Test generated for a dsx_vm compilation" << endl;
      res << " * NB_LOCKS : " << nb_locks << endl;
      res << " * NB_VARS : " << nb_vars << endl;
      if (lock_in_line) {
         res << " * LOCKS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * LOCKS PLACEMENT : vertical" << endl;
      }
      if (vars_in_line) {
         res << " * VARS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * VARS PLACEMENT : vertical" << endl;
      }
      res << " * NB_MAX_INCRS : " << nb_max_incr << endl;
      res << " * LINE_SIZE : " << LINE_SIZE << endl;
      res << " */" << endl;
      res << endl;
      res << "int * tab;" << endl;
      res << endl;

      for (int i = 1; i < nb_threads; i++) {
         res << threads[i]->write_task();
      }
      
      res << endl;
      res << endl;
      res << "FUNC(test_llsc_no_tty_func) {" << endl;
      res << endl;
      res << "   srl_memspace_t memspace = SRL_GET_MEMSPACE(table);" << endl;
      res << "   tab = (int *) SRL_MEMSPACE_ADDR(memspace);" << endl;
      res << "   srl_barrier_t barrier = SRL_GET_BARRIER(barrier);" << endl;
      res << "   srl_barrier_t barrier2 = SRL_GET_BARRIER(barrier2);" << endl;
      res << "   int thread_id = SRL_GET_CONST(id);" << endl;
      res << endl;
      res << "   barrier_wait(barrier);" << endl;
      res << endl;
      res << "   if (thread_id == 1) {" << endl;
      res << "      run1();" << endl;
      res << "   }" << endl;
      for (int i = 2; i < nb_threads; i++) {
         res << "   else if (thread_id == " << i << ") {" << endl;
         res << "      run" << i << "();" << endl;
         res << "   }" << endl;
      }
      res << "   else {" << endl;
      res << "      srl_assert(0);" << endl;
      res << "   }" << endl;
      res << endl;
      res << "   barrier_wait(barrier2);" << endl;
      res << endl;
      res << "   srl_exit();" << endl;
      res << endl;
      res << "}" << endl;
      res << endl;

      return res.str();
   }


   string write_output() {
      cout << tab_size;
      stringstream res;
      res << endl;
      res << "#include <stdio.h>" << endl;
      res << "#include <assert.h>" << endl;
      res << endl;
      res << "/*" << endl;
      res << " * Test generated for a posix execution" << endl;
      res << " * NB_LOCKS : " << nb_locks << endl;
      res << " * NB_VARS : " << nb_vars << endl;
      if (lock_in_line) {
         res << " * LOCKS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * LOCKS PLACEMENT : vertical" << endl;
      }
      if (vars_in_line) {
         res << " * VARS PLACEMENT : horizontal" << endl;
      }
      else {
         res << " * VARS PLACEMENT : vertical" << endl;
      }
      res << " * NB_MAX_INCRS : " << nb_max_incr << endl;
      res << " * LINE_SIZE : " << LINE_SIZE << endl;
      res << " */" << endl;
      res << endl;
      res << "volatile int tab[" << tab_size << "];" << endl;
      res << endl;
      res << endl;

      
      for (int i = 0; i < nb_threads; i++) {
         res << threads[i]->write_output();
      }
      
      res << "int main() {" << endl;
      res << endl;
      res << "   int i;" << endl;
      res << endl;
      res << "   /* Version native séquentielle */" << endl;
      res << endl;
      res << "   // Initialisation du tableau" << endl;
      res << "   for (i = 0; i < " << tab_size << "; i++) {" << endl;
      res << "      tab[i] = 0;" << endl;
      res << "   }" << endl;
      res << endl;
      res << "   run0();" << endl;
      for (int i = 1; i < nb_threads; i++){
         res << "   run" << i << "();" << endl;
      }
      res << endl;
      res << "   for (i = 0; i < " << tab_size << "; i++) {" << endl;
      res << "      if (tab[i] != 0) {" << endl;
      res << "         printf(\"tab[%d] final : %d\\n\", i, tab[i]);" << endl;
      res << "      }" << endl;
      res << "   }" << endl;
      res << endl;
      res << "   return 0;" << endl;
      res << "}" << endl;
      res << endl;

      return res.str();
    }

};

#endif





