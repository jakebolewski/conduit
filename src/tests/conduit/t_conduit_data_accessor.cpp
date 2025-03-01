// Copyright (c) Lawrence Livermore National Security, LLC and other Conduit
// Project developers. See top-level LICENSE AND COPYRIGHT files for dates and
// other details. No copyright assignment is required to contribute to Conduit.

//-----------------------------------------------------------------------------
///
/// file: conduit_array.cpp
///
//-----------------------------------------------------------------------------

#include "conduit.hpp"

#include <iostream>
#include "gtest/gtest.h"

using namespace conduit;

//-----------------------------------------------------------------------------
TEST(conduit_data_accessor, value)
{

    Node n;
    n.set((int8)10);

    int8_accessor  i8_acc  = n.value();
    int16_accessor i16_acc = n.value();
    int32_accessor i32_acc = n.value();
    int64_accessor i64_acc = n.value();

    uint8_accessor  ui8_acc  = n.value();
    uint16_accessor ui16_acc = n.value();
    uint32_accessor ui32_acc = n.value();
    uint64_accessor ui64_acc = n.value();
    
    
    float32_accessor f32_acc = n.value();
    float64_accessor f64_acc = n.value();
    
    EXPECT_EQ(i8_acc[0],(int8)(10));
    EXPECT_EQ(i16_acc[0],(int16)(10));
    EXPECT_EQ(i32_acc[0],(int32)(10));
    EXPECT_EQ(i64_acc[0],(int64)(10));
    
    
    EXPECT_EQ(ui8_acc[0],(uint8)(10));
    EXPECT_EQ(ui16_acc[0],(uint16)(10));
    EXPECT_EQ(ui32_acc[0],(uint32)(10));
    EXPECT_EQ(ui64_acc[0],(uint64)(10));

    EXPECT_EQ(f32_acc[0],(float32)(10));
    EXPECT_EQ(f64_acc[0],(float64)(10));

}


//-----------------------------------------------------------------------------
TEST(conduit_data_accessor, as_bitwidth_style)
{

    Node n;
    n.set((int8)10);

    int8_accessor  i8_acc  = n.as_int8_accessor();
    int16_accessor i16_acc = n.as_int16_accessor();
    int32_accessor i32_acc = n.as_int32_accessor();
    int64_accessor i64_acc = n.as_int64_accessor();

    uint8_accessor  ui8_acc  = n.as_uint8_accessor();
    uint16_accessor ui16_acc = n.as_uint16_accessor();
    uint32_accessor ui32_acc = n.as_uint32_accessor();
    uint64_accessor ui64_acc = n.as_uint64_accessor();
    
    
    float32_accessor f32_acc = n.as_float32_accessor();
    float64_accessor f64_acc = n.as_float64_accessor();
    
    EXPECT_EQ(i8_acc[0],(int8)(10));
    EXPECT_EQ(i16_acc[0],(int16)(10));
    EXPECT_EQ(i32_acc[0],(int32)(10));
    EXPECT_EQ(i64_acc[0],(int64)(10));
    
    
    EXPECT_EQ(ui8_acc[0],(uint8)(10));
    EXPECT_EQ(ui16_acc[0],(uint16)(10));
    EXPECT_EQ(ui32_acc[0],(uint32)(10));
    EXPECT_EQ(ui64_acc[0],(uint64)(10));

    EXPECT_EQ(f32_acc[0],(float32)(10));
    EXPECT_EQ(f64_acc[0],(float64)(10));

}


//-----------------------------------------------------------------------------
TEST(conduit_data_accessor, as_cstyle)
{

    Node n;
    n.set((int8)10);

    char_accessor         c_acc  = n.as_char_accessor();
    signed_char_accessor  sc_acc = n.as_signed_char_accessor();
    signed_short_accessor ss_acc = n.as_signed_short_accessor();
    signed_int_accessor   si_acc = n.as_signed_int_accessor();
    signed_long_accessor  sl_acc = n.as_signed_long_accessor();

#ifdef CONDUIT_HAS_LONG_LONG
    signed_long_long_accessor  sll_acc = n.as_signed_long_long_accessor();
#endif

    unsigned_char_accessor  usc_acc = n.as_unsigned_char_accessor();
    unsigned_short_accessor uss_acc = n.as_unsigned_short_accessor();
    unsigned_int_accessor   usi_acc = n.as_unsigned_int_accessor();
    unsigned_long_accessor  usl_acc = n.as_unsigned_long_accessor();

#ifdef CONDUIT_HAS_LONG_LONG
    unsigned_long_long_accessor  usll_acc = n.as_unsigned_long_long_accessor();
#endif

    float_accessor  f_acc = n.as_float_accessor();
    double_accessor d_acc = n.as_double_accessor();

#ifdef CONDUIT_USE_LONG_DOUBLE
    long_double_accessor  ld_acc = n.as_long_double_accessor();
#endif

    EXPECT_EQ(c_acc[0],(char)(10));
    EXPECT_EQ(sc_acc[0],(signed char)(10));
    EXPECT_EQ(ss_acc[0],(signed short)(10));
    EXPECT_EQ(si_acc[0],(signed int)(10));
    EXPECT_EQ(sl_acc[0],(signed long)(10));

#ifdef CONDUIT_HAS_LONG_LONG
    EXPECT_EQ(sll_acc[0],(signed long long)(10));
#endif

    EXPECT_EQ(usc_acc[0],(unsigned char)(10));
    EXPECT_EQ(uss_acc[0],(unsigned short)(10));
    EXPECT_EQ(usi_acc[0],(unsigned int)(10));
    EXPECT_EQ(usl_acc[0],(unsigned long)(10));

#ifdef CONDUIT_HAS_LONG_LONG
    EXPECT_EQ(usll_acc[0],(unsigned long long)(10));
#endif

    EXPECT_EQ(f_acc[0],(float)(10));
    EXPECT_EQ(d_acc[0],(double)(10));

#ifdef CONDUIT_USE_LONG_DOUBLE
    EXPECT_EQ(ld_acc[0],(long double)(10));
#endif 

}





