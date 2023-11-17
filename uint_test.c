#include <stdint.h>
#include <stdio.h>

int main()
{
    uint16_t a1 = 1;
	uint16_t b1 = a1 - 2;
	uint8_t c1 = 2;
    if((a1 - b1) - c1 >= 0) // (int)-65534 - (int)2 = -65536
	{
		printf("1\n");
	}
    printf("%ld\n",sizeof(a1-b1)); // 4

    uint16_t a2 = 1;
	uint16_t b2 = a2 - 2;
	uint32_t c2 = 2;
    if((a2 - b2) - c2 >= 0)//(int)-65534 - (uint32)2 = (uint32)4294901762 - (uint32)2 = 4294901760
	{
		printf("2\n");
	}

    uint32_t a3 = 1;
	uint32_t b3 = a3 - 2;
	uint16_t c3 = 2;
    if((a3 - b3) - c3 >= 0)//(uint32)1-(uint32)4294967295 - (uint16)2 = (uint32)2(溢出) - (uint32)2 = 0
	{
		printf("3\n");
	}
    
    uint64_t a4 = 1;
	uint32_t b4 = a4 - 2;
    printf("%lld",sizeof(a4-b4)); // 8
    return 0;
}