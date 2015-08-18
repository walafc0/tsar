/*
 * Outil pour la génération de test de la table LL/SC
 * @Author QM
 */

#include "Program.hpp"
#include "config.h"

#include <time.h>
#include <cstdlib>
#include <iostream>
#include <cstdio>

int main(int argc, char** argv){

   if (argc != 9){
      printf("usage: genere_test <nb_procs> <nb_max_incr> <nb_locks_and_vars> <locks_horizontal> <vars_horizontal> <native_filename> <main_task_filename> <task_no_tty_filename>\n");
      exit(1);
   }

   srand(time(NULL));

   FILE * native_file;
   FILE * main_task_file;
   FILE * task_no_tty_file;
   
   native_file = fopen(argv[6], "w");
   main_task_file = fopen(argv[7], "w");
   task_no_tty_file = fopen(argv[8], "w");

   const int nb_procs = atoi(argv[1]);
   const int nb_max_incr = atoi(argv[2]);
   const int nb_locks = atoi(argv[3]);

   const bool lock_horizontal = atoi(argv[4]);
   const bool vars_horizontal = atoi(argv[5]);

	const int line_size = LINE_SIZE;

   Program prog(nb_procs, nb_locks, nb_max_incr, line_size, lock_horizontal, vars_horizontal);

   string s = prog.write_output();
   fprintf(native_file, "%s", s.c_str());
   fclose(native_file);

   s = prog.write_main_task();
   fprintf(main_task_file, "%s", s.c_str());
   fclose(main_task_file);
 
   s = prog.write_task_no_tty();
   fprintf(task_no_tty_file, "%s", s.c_str());
   fclose(task_no_tty_file);  

   return 0;
}



