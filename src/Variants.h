#ifndef VARIANTS_H
#define VARIANTS_H


//#define VARIANT_4_ANDABOVE //never comment out
//#define VARIANT_3_ANDABOVE //comment out if this sketch should be compiled according to Variant 4 at max
//#define VARIANT_2_ANDABOVE // ...                                                       Variant 3 at max
//#define VARIANT_1_ANDABOVE // ...                                                       Variant 2 at max
//#define VARIANT_0_ANDABOVE // ...                                                       Variant 1 at max, leave in code if sketch should be compiled for full OCPP

#define PM true //true if performance shall be taken and outputted
#define DEBUG_OUTPUT false
#define DEBUG_APP_LAY true

//#define OCPP_SERVER //comment out if this should be compiled as server

//#define VARIANT_4
//#define VARIANT_3
//#define VARIANT_2
//#define VARIANT_1
#define VARIANT_0

#ifdef VARIANT_4
#define VARIANT_NR 4
#define VARIANT_4_ANDABOVE
#endif

#ifdef VARIANT_3
#define VARIANT_NR 3
#define VARIANT_4_ANDABOVE
#define VARIANT_3_ANDABOVE
#endif

#ifdef VARIANT_2
#define VARIANT_NR 2
#define VARIANT_4_ANDABOVE
#define VARIANT_3_ANDABOVE
#define VARIANT_2_ANDABOVE
#endif

#ifdef VARIANT_1
#define VARIANT_NR 1
#define VARIANT_4_ANDABOVE
#define VARIANT_3_ANDABOVE
#define VARIANT_2_ANDABOVE
#define VARIANT_1_ANDABOVE
#endif

#ifdef VARIANT_0
#define VARIANT_NR 0
#define VARIANT_4_ANDABOVE
#define VARIANT_3_ANDABOVE
#define VARIANT_2_ANDABOVE
#define VARIANT_1_ANDABOVE
#define VARIANT_0_ANDABOVE
#endif

#endif
