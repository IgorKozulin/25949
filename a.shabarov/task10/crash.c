int main(void) {
    int *ptr = 0;
    *ptr = 42; // Вызовет Segmentation fault
    return 0;
}
