#ifndef SOS_CRT_SYMBOLS_HPP
#define SOS_CRT_SYMBOLS_HPP

#include <var/String.hpp>

var::String sos_crt_symbols_get_signature();
u32 sos_crt_symbols_get_count();
var::String sos_crt_symbols_get_entry(u32 value);


#endif // SOS_CRT_SYMBOLS_HPP
