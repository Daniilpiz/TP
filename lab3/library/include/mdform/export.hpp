// Макрос экспорта функций библиотеки.
//
//  - Статическая библиотека (.lib) и обычная сборка ЛР2: макрос пустой.
//  - DLL: при сборке самой DLL определён MDFORM_BUILDING (+ MDFORM_SHARED) -> dllexport,
//         в проекте-потребителе определён только MDFORM_SHARED -> dllimport.
#pragma once

#if defined(_WIN32) && defined(MDFORM_SHARED)
#  if defined(MDFORM_BUILDING)
#    define MDFORM_API __declspec(dllexport)
#  else
#    define MDFORM_API __declspec(dllimport)
#  endif
#else
#  define MDFORM_API
#endif
