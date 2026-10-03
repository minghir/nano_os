#include "string.h"

// O funcție simplă de conversie string -> integer
 int simple_atoi(const char* str) {
    if (!str) return 0;
    
    int res = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res * sign;
}

 char* simple_itoa(int value, char* buffer) {
    if (!buffer) return 0;
    
    char* ptr = buffer;
    
    if (value == -2147483648) {
        const char* min_str = "-2147483648";
        int i = 0;
        while (min_str[i] != '\0') {
            buffer[i] = min_str[i];
            i++;
        }
        buffer[i] = '\0';
        return buffer;
    }

    if (value < 0) {
        *ptr++ = '-';
        value = -value;
    }

    char* start = ptr;

    do {
        int digit = value % 10;
        *ptr++ = '0' + digit;
        value /= 10;
    } while (value > 0);

    *ptr = '\0';

    char* end = ptr - 1;
    while (start < end) {
        char tmp = *start;
        *start = *end;
        *end = tmp;
        start++;
        end--;
    }

    return buffer;
}

 int is_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

 char* trim(char* str) {
    if (!str) return str;
    while (is_space(*str)) {
        str++;
    }
    if (*str == '\0') {
        return str;
    }
    char* end = str;
    while (*end != '\0') {
        end++;
    }
    end--;
    while (end > str && is_space(*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

 char* ltrim(char* str) {
    if (!str) return str;
    while (is_space(*str)) {
        str++;
    }
    return str;
}

 char* rtrim(char* str) {
    if (!str) return str;
    char* end = str;
    while (*end != '\0') {
        end++;
    }
    if (end == str) {
        return str;
    }
    end--;
    while (end >= str && is_space(*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

 void* memset(void* dest, int val, int count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count-- > 0) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

 void* memcpy(void* dest, const void* src, int count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

 char* string_copy(char* dest, const char* src) {
    char* original_dest = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return original_dest;
}

 void string_concat(char* dest, const char* src) {
    while (*dest != '\0') {
        dest++;
    }
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
}

 int starts_with(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

 void print_number(uint32_t value) {
    char buffer[11];
    int index = 10;
    buffer[index] = 0;

    do {
        buffer[--index] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    print(&buffer[index]);
}

 int string_length(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

 int string_contains(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        int j = 0;
        while (str[i + j] != '\0' && substr[j] != '\0' && str[i + j] == substr[j]) {
            j++;
        }
        if (substr[j] == '\0') return 1;
    }
    return 0;
}

 void _int_to_dec(int64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int is_negative = 0;
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    char tmp[32];
    int idx = 0;
    while (val > 0) {
        tmp[idx++] = '0' + (val % 10);
        val /= 10;
    }
    int i = 0;
    if (is_negative) {
        buf[i++] = '-';
    }
    while (idx > 0) {
        buf[i++] = tmp[--idx];
    }
    buf[i] = '\0';
}

 void _uint_to_hex(uint64_t val, char* buf, int uppercase) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[32];
    int idx = 0;
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    while (val > 0) {
        tmp[idx++] = digits[val % 16];
        val /= 16;
    }
    int i = 0;
    while (idx > 0) {
        buf[i++] = tmp[--idx];
    }
    buf[i] = '\0';
}

 int snprintf(char* str, uint32_t size, const char* format, ...) {
    if (!str || size == 0 || !format) return 0;

    va_list args;
    va_start(args, format);

    uint32_t written = 0;
    char* ptr = str;
    uint32_t max_len = size - 1;

    int i = 0;
    while (format[i] != '\0' && written < max_len) {
        if (format[i] == '%' && format[i+1] != '\0') {
            i++;
            char spec = format[i];

            char conv_buf[32];
            char* src = conv_buf;

            if (spec == 'd' || spec == 'i') {
                int64_t val = va_arg(args, int);
                _int_to_dec(val, conv_buf);
            } 
            else if (spec == 'x' || spec == 'X') {
                uint64_t val = va_arg(args, uint64_t);
                _uint_to_hex(val, conv_buf, (spec == 'X'));
            } 
            else if (spec == 'p') {
                *ptr++ = '0'; written++;
                if (written < max_len) { *ptr++ = 'x'; written++; }
                uint64_t val = (uint64_t)va_arg(args, void*);
                _uint_to_hex(val, conv_buf, 0);
            } 
            else if (spec == 's') {
                char* s = va_arg(args, char*);
                src = s ? s : "(null)";
            } 
            else if (spec == 'c') {
                conv_buf[0] = (char)va_arg(args, int);
                conv_buf[1] = '\0';
            } 
            else {
                conv_buf[0] = '%';
                conv_buf[1] = spec;
                conv_buf[2] = '\0';
            }

            while (*src != '\0' && written < max_len) {
                *ptr++ = *src++;
                written++;
            }
            i++;
        } else {
            *ptr++ = format[i++];
            written++;
        }
    }

    *ptr = '\0';
    va_end(args);
    return (int)written;
}



