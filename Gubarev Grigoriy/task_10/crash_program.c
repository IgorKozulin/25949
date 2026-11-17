/* Test helper for run_command: dies with SIGSEGV by writing through a null pointer. */
int main(void)
{
    volatile int *pointer = 0;

    *pointer = 1;
    return 0;
}
