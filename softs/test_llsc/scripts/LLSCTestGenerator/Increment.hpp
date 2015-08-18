
#ifndef _increment_hpp_
#define _increment_hpp_


#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

#include "functions.h"

using namespace std;

class Increment {
   int lock_index;
   int var_index;
   int line_size;

   public:

   Increment(int nb_locks, int line_size, bool lock_in_line, bool vars_in_line){
      this->line_size = line_size;

      int index = randint(0, nb_locks - 1);
      int lock_size = (lock_in_line ? ceil((float) nb_locks / line_size) * line_size : nb_locks * line_size);

      lock_index = (lock_in_line ? index : index * line_size);
      var_index = (vars_in_line ? lock_size + index : lock_size + index * line_size);
   }

   ~Increment() {}

   string write_output() {
      stringstream res;
      res << "   tab[" << var_index << "]++;" << endl;

      return res.str();
   }

   string write_task() {
      stringstream res;
      res << "   take_lock(&tab[" << lock_index << "]);" << endl;
      res << "   tab[" << var_index << "]++;" << endl;
      res << "   release_lock(&tab[" << lock_index << "]);" << endl;

      return res.str();
   }

};

#endif

