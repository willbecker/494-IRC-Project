#include "structs.h"

int serialize_set_name(struct set_name in, char* out){
    sprintf(out, "%i,%i,%s", in.header.type, in.header.length, in.name);
    return 0;
}
