#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

#define container_of(ptr, type, member) ({ 				\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);		\
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define CIX_STRUCT_PACKED __attribute__((packed))
