#pragma once

#ifdef USING_BML_PLUS
# include <BML/BMLAll.h>
# ifndef m_bml
#  define m_bml m_BML
#  define m_sprite m_Sprite
#  define VT21_REF(x) &(x)
# endif
typedef const char* iCKSTRING;
#else
# include <BML/BMLAll.h>
# define VT21_REF(x) (x)
typedef CKSTRING iCKSTRING;
#endif
/* 兼容 BML+ 全版本：
 *  - v0.3.0 ~ v0.3.2 使用 BML_*_VER 宏名，BMLVersion 的第三字段名为 build
 *  - v0.3.3+       使用 BML_*_VERSION 宏名，BMLVersion 的第三字段名为 patch
 * 用 defined(BML_MINOR_VERSION) 判断版本代际，避免在旧版头文件上编译出错。 */
#if defined(USING_BML_PLUS) && defined(BML_MINOR_VERSION) && BML_MINOR_VERSION >= 3
# define BML_BUILD_VAR_NAME(x) (x).patch
#else
# define BML_BUILD_VAR_NAME(x) (x).build
#endif
