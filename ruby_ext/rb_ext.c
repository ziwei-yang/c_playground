#include "ruby.h"

#include "../urn.h"

//////////////////////////////////////////
// Ruby Methods below.
//////////////////////////////////////////

// Defining a space for information and references about the module to be stored internally
VALUE URN_MKTDATA = Qnil;

VALUE method_mkdata_shm_index(VALUE self);

// The initialization method for this module
// Prototype for the initialization method - Ruby calls this, not you
void Init_urn_mktdata() {
	URN_MKTDATA = rb_define_module("URN_MKTDATA");
//	rb_define_method(URN_MKTDATA, "mktdata_shm", method_test1, 0);

//	rb_define_method(URN_MKTDATA, "test1", method_test1, 0);
//	rb_define_method(URN_MKTDATA, "add", method_add, 2);
//	rb_define_method(URN_MKTDATA, "attach_shm", method_attach_shm, 0);
//	rb_define_method(URN_MKTDATA, "detach_shm", method_detach_shm, 0);
//	rb_define_method(URN_MKTDATA, "read_shm", method_read_shm, 0);
}

VALUE method_mkdata_shm_index(VALUE self) {
	if (RB_TYPE_P(str, T_STRING) == 1) {
		return rb_sprintf("String length: %ld", RSTRING_LEN(str));
	}
	return Qnil;
}

// Prototype for our method 'test1' - methods are prefixed by 'method_' here
VALUE method_test1(VALUE self);
VALUE method_add(VALUE, VALUE, VALUE);

#define SHM_KEY 0x1234
#define BUF_SIZE 1024
struct shmseg {
   int cnt;
   int complete;
   char buf[BUF_SIZE];
};
struct shmseg *shmp = NULL;
int shmid = -1;
VALUE method_attach_shm(VALUE self) {
   shmid = shmget(SHM_KEY, sizeof(struct shmseg), 0644|IPC_CREAT);
   if (shmid == -1) {
      perror("Shared memory");
      return Qnil;
   }
   printf("from ext shmid %d\n", shmid);
   
   // Attach to the segment to get a pointer to it.
   shmp = shmat(shmid, NULL, 0);
   if (shmp == (void *) -1) {
      perror("Shared memory attach");
      return Qnil;
   }

   printf("Shared memory attached\n");
   return INT2NUM(shmid);
}

VALUE method_detach_shm(VALUE self) {
   return INT2NUM(shmdt(shmp));
}

VALUE method_read_shm(VALUE self) {
	VALUE row = rb_ary_new_capa(3);
	VALUE subrow = rb_ary_new_capa(3);
	rb_ary_push(subrow, DBL2NUM(shmp->buf[0]));
	rb_ary_push(subrow, DBL2NUM(shmp->buf[1]));
	rb_ary_push(subrow, DBL2NUM(shmp->buf[2]));
	rb_ary_push(row, subrow);
	return row;
}

VALUE method_test1(VALUE self) {
	VALUE row = rb_ary_new_capa(3);
	VALUE subrow = rb_ary_new_capa(3);
	rb_ary_push(subrow, DBL2NUM(1.234567899));
	rb_ary_push(row, subrow);
	return row;
}

// This is the method we added to test out passing parameters
VALUE method_add(VALUE self, VALUE first, VALUE second) {
	int a = NUM2INT(first);
	int b = NUM2INT(second);
	return INT2NUM(a + b);
}
