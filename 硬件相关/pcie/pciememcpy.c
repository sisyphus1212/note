#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>

#define REG_NUM         16//32 * 4 bits

// Error handling macro
#define HANDLE_ERROR(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Function to calculate the time difference in nanoseconds
long long time_diff_ns(struct timespec *start, struct timespec *end)
{
    long long diff = (end->tv_sec - start->tv_sec) * 1000000000L + (end->tv_nsec - start->tv_nsec);
    return diff;
}
void *memcpy_inline(void *dest, const void *src, size_t n) {
    __asm__ volatile (
        "1: \n\t"
        "ldrb    w2, [%[src]]   \n\t"  // 从源地址加载一个字节到 r2
        "strb    w2, [%[dest]]  \n\t"  // 将字节存储到目标地址
        "add     %[src], %[src], #1 \n\t"  // 源地址增加 1
        "add     %[dest], %[dest], #1 \n\t" // 目标地址增加 1
        "subs    %[count], %[count], #1 \n\t" // 计数器减 1
        "bne     1b              \n\t"  // 如果计数器不为 0，则跳转回 1
        : [dest] "+r"(dest), [src] "+r"(src), [count] "+r"(n)
        : // No output operands
        : "r2", "cc"  // clobbered registers
    );
    return dest; // 返回目标地址
}
// Function to read data from a specified PCI address and offset
void pci_memcpy(const char *resource_path, uintptr_t offset, size_t reg_count)
{
    struct timespec start_time, end_time;
    int fd;
    unsigned char *mapped_memory;
    void *address;

    // Each register is 32 bits (4 bytes), so convert register count to byte size
    size_t size = reg_count * 4;

    // Open the PCI resource file
    fd = open(resource_path, O_RDWR);
    if (fd == -1) {
        HANDLE_ERROR("Failed to open PCI resource file");
    }


    // uintptr_t aligned_offset = offset & ~(getpagesize() - 1);
    // Get the system's page size
    size_t page_size = getpagesize();

    // // Align the offset to the nearest page boundary
    // offset &= ~(page_size - 1);  // Align to page boundary
    // printf("hello %ld, %d\r\n", size, offset);
    // Map the PCI resource to memory
    uintptr_t aligned_offset = offset & ~(page_size - 1);
    address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, aligned_offset);
    if (address == MAP_FAILED) {
        HANDLE_ERROR("Failed to mmap PCI resource");
    }

    uintptr_t actual_offset = offset - aligned_offset;
    // Copy data from the mapped memory in batches of 4 registers ((REG_NUM * 4) bytes)
    mapped_memory = (unsigned char *)(address + actual_offset);
    size_t remaining_size = size;
    unsigned char buffer[REG_NUM * 4];  // Buffer to hold up to 4 registers ((REG_NUM * 4) bytes)

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    memcpy_inline(buffer, mapped_memory, 4);
    //while (remaining_size >= (REG_NUM * 4)) {
    //    // Copy (REG_NUM * 4) bytes (4 registers) at a time
    //    memcpy(buffer, mapped_memory, (REG_NUM * 4));
    //    mapped_memory += (REG_NUM * 4);
    //    remaining_size -= (REG_NUM * 4);
    //
    //    // Print the copied data (in hexadecimal format)
    //    printf("Copied Data (in Hex): ");
    //    for (size_t i = 0; i < (REG_NUM * 4); i++) {
    //        printf("%02x ", buffer[i]);
    //    }
    //    printf("\n");
    //}

    // If there's any remaining data (less than (REG_NUM * 4) bytes), copy it all at once
    //if (remaining_size > 0) {
    //    memcpy(buffer, mapped_memory, remaining_size);
    //    printf("Remaining Data (in Hex): ");
    //    for (size_t i = 0; i < remaining_size; i++) {
    //        printf("%02x ", buffer[i]);
    //    }
    //    printf("\n");
    //}
    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // Unmap the memory
    if (munmap(address, size) == -1) {
        HANDLE_ERROR("Failed to munmap PCI resource");
    }

    // Close the file descriptor
    close(fd);

    // Print the time taken in nanoseconds
    long long elapsed_time_ns = time_diff_ns(&start_time, &end_time);
    printf("Read operation took %lld ns\n", elapsed_time_ns);
}

// Function to write data to a specified PCI address and offset
void pci_memwrite(const char *resource_path, uintptr_t offset, size_t reg_count, uint32_t *data)
{
    struct timespec start_time, end_time;
    int fd;
    unsigned char *mapped_memory;
    void *address;

    // Each register is 32 bits (4 bytes), so convert register count to byte size
    size_t size = reg_count * 4;

    // Start timing
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // Open the PCI resource file
    fd = open(resource_path, O_RDWR);
    if (fd == -1) {
        HANDLE_ERROR("Failed to open PCI resource file");
    }

    // Get the system's page size
    size_t page_size = getpagesize();

    // Map the PCI resource to memory
    uintptr_t aligned_offset = offset & ~(page_size - 1);
    // Map the PCI resource to memory
    address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, aligned_offset);
    if (address == MAP_FAILED) {
        HANDLE_ERROR("Failed to mmap PCI resource");
    }

    uintptr_t actual_offset = offset - aligned_offset;
    // Write data to the mapped memory in batches of REG_NUM registers (REG_NUM * 4 bytes)
    mapped_memory = (unsigned char *)(address + actual_offset);
    size_t remaining_size = size;
    size_t index = 0;

    while (remaining_size >= (REG_NUM * 4)) {
        memcpy(mapped_memory, &data[index], (REG_NUM * 4));
        mapped_memory += (REG_NUM * 4);
        index += REG_NUM;
        remaining_size -= (REG_NUM * 4);
    }

    // If there's any remaining data (less than REG_NUM * 4 bytes), write it all at once
    if (remaining_size > 0) {
        memcpy(mapped_memory, &data[index], remaining_size);
    }

    // Unmap the memory
    if (munmap(address, size) == -1) {
        HANDLE_ERROR("Failed to munmap PCI resource");
    }

    // Close the file descriptor
    close(fd);

    // End timing
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // Print the time taken in nanoseconds
    long long elapsed_time_ns = time_diff_ns(&start_time, &end_time);
    printf("Write operation took %lld ns\n", elapsed_time_ns);
}

// Function to parse the array of hex values from the command line
int parse_hex_array(char *input, uint32_t *output, size_t max_size)
{
    char *token;
    size_t count = 0;

    // Remove the square brackets
    if (input[0] == '[') input++;
    if (input[strlen(input) - 1] == ']') input[strlen(input) - 1] = '\0';

    // Tokenize the input based on spaces
    token = strtok(input, " ");
    while (token != NULL && count < max_size) {
        if (sscanf(token, "0x%x", &output[count]) != 1) {
            fprintf(stderr, "Invalid hex value: %s\n", token);
            return -1;
        }
        count++;
        token = strtok(NULL, " ");
    }

    return count;
}

int main(int argc, char *argv[])
{
    // Ensure the correct number of arguments is provided
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "Usage: %s <resource_path> <offset> <register_count> <data_array(optional)> <r/w>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get the resource path, offset, and register count from the command line arguments
    const char *resource_path = argv[1];
    uintptr_t offset = strtoull(argv[2], NULL, 0); // Convert the offset to hexadecimal
    size_t reg_count = strtoul(argv[3], NULL, 0); // Get the number of registers to copy or write
    char *operation = argv[argc - 1]; // 'r' or 'w'

    if (strcmp(operation, "r") == 0) {
        // Perform the memory copy from the PCI device (read)
        pci_memcpy(resource_path, offset, reg_count);
    } else if (strcmp(operation, "w") == 0) {
        if (argc != 6) {
            fprintf(stderr, "For write operation, you must provide a data array.\n");
            exit(EXIT_FAILURE);
        }

        // Parse the hex array from the input string
        uint32_t data[reg_count];
        int parsed_count = parse_hex_array(argv[4], data, reg_count);
        if (parsed_count != reg_count) {
            fprintf(stderr, "Error parsing data array.\n");
            exit(EXIT_FAILURE);
        }

        // Perform the memory write to the PCI device
        pci_memwrite(resource_path, offset, reg_count, data);
    } else {
        fprintf(stderr, "Invalid operation. Use 'r' for read or 'w' for write.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}