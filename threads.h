#include<pthread.h>

//function declarations
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
void schedule();
void pthread_exit(void *value_ptr);
pthread_t pthread_self();

//provided functions from ec440
void *start_thunk();
unsigned long int ptr_mangle(unsigned long int p);
unsigned long int ptr_demangle(unsigned long int p);