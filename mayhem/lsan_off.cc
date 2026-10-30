// Leaks are not the bug class this fleet fuzzes for; ASan/UBSan stay fully active.
extern "C" int __lsan_is_turned_off() { return 1; }
