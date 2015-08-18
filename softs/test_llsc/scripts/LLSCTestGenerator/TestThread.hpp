
#ifndef _testthread_hpp_
#define _testthread_hpp_

#include <iostream>
#include <sstream>
#include <list>

#include "Increment.hpp"

using namespace std;

class TestThread {

   int proc_id;
   std::list<Increment *> requests;

   public:

   TestThread(int nb_locks, int nb_max_accesses, int line_size, bool lock_in_line, bool vars_in_line, int proc_id) {
      this->proc_id = proc_id;
      const int nb_accesses = randint(1, nb_max_accesses);
      for (int i = 0; i < nb_accesses; i++) {
         Increment * t;
         t = new Increment(nb_locks, line_size, lock_in_line, vars_in_line);
         requests.push_back(t);
      }
   }

   ~TestThread() {
      std::list<Increment *>::iterator it;
      for (it = requests.begin(); it != requests.end(); it++) {
         delete (*it);
      }
      requests.clear();
   }
   
   string write_output() {
      stringstream res;
      res << "void run" << proc_id << "() {" << endl;

      std::list<Increment *>::iterator it;
      for (it = requests.begin(); it != requests.end(); it++) {
         res << (*it)->write_output();
      }

      res << "}" << endl;
      res << endl;

      return res.str();
   }


   string write_task() {
      stringstream res;
      res << "__attribute__((constructor)) void run" << proc_id << "() {" << endl;

      std::list<Increment *>::iterator it;
      for (it = requests.begin(); it != requests.end(); it++) {
         res << (*it)->write_task();
      }

      res << "}" << endl;
      res << endl;

      return res.str();
   }
};

#endif
