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
    printf("%lld  %llu\n",sizeof(a4-b4),(a4-b4)); // 8



	//printf首先读后边的数据进栈，从右向左压栈。然后依次出栈从左往右处理转换符
	uint16_t a5 = 1;
	int32_t c5 = 2;
	uint16_t b5 = a5 - c5; //(int32)1 - (int32)2 = (int32)-1 = (uint16)65535

	printf("%d %ld %lu %lld %llu ###%llu\n", (a5-c5), (a5-c5),(a5-c5),(a5-c5),(a5-c5), b5);
	//-1 -1 4294967295 4294967295 4294967295 ###140698833715199

	printf("###%llu %d %ld %lu %lld %llu \n", b5, (a5-c5), (a5-c5),(a5-c5),(a5-c5),(a5-c5));
	//###65535 -1 -1 4294967295 4294967295 140703128616959

	printf("%d %ld %lu %lld %llu %u ###%llu\n", (a5-c5), (a5-c5),(a5-c5),(a5-c5),(a5-c5), 1, b5);
	//-1 -1 4294967295 4294967295 4294967295 1 ###65535

	printf("###%llu %u %d\n",b5,b5,b5);//###65535 65535 65535
	


    return 0;
}