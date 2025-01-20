#include "object.h"
#include <stdint.h>
#include <sys/auxv.h>

uintptr_t start_rsp;

extern _Noreturn void rtld_handover(uintptr_t rip, uintptr_t rsp);

int main(void) {
    init_objects();

    init_object(&vdso_object);
    init_object(&rtld_object);
    process_dependencies(&exec_object);

    rtld_handover(getauxval(AT_ENTRY), start_rsp);
}
