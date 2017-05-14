#ifndef INJECT_H
#define INJECT_H

#define DLLEXPORT __declspec (dllexport)

extern "C"{
DLLEXPORT void __stdcall Init(void);
}

#endif
