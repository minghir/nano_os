#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "string.h"
#include "nano_libc.h"
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
/*
 void* memset(void* dest, int val, size_t count) {
    unsigned char* ptr = (unsigned char*)dest;
    while (count-- > 0) {
        *ptr++ = (unsigned char)val;
    }
    return dest;
}

 void* memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (count-- > 0) {
        *d++ = *s++;
    }
    return dest;
}
*/


void* memset(void* dest, int val, size_t count) {
    char* d = (char*)dest;
    uint8_t v = (uint8_t)val;

    if (count < 16) {
        while (count-- > 0) *d++ = v;
        return dest;
    }

    while (((uintptr_t)d & 0xF) != 0 && count > 0) {
        *d++ = v;
        count--;
    }

    uint64_t pattern = v;
    pattern |= (pattern << 8);
    pattern |= (pattern << 16);
    pattern |= (pattern << 32);

    __asm__ volatile(
        "movd %1, %%xmm0\n\t"
        "pshufd $0, %%xmm0, %%xmm0\n\t"
        "1:\n\t"
        "cmp $64, %2\n\t"
        "jl 2f\n\t"
        "movaps %%xmm0, (%0)\n\t"
        "movaps %%xmm0, 16(%0)\n\t"
        "movaps %%xmm0, 32(%0)\n\t"
        "movaps %%xmm0, 48(%0)\n\t"
        "add $64, %0\n\t"
        "sub $64, %2\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        : "+r" (d), "+r" (pattern), "+r" (count)
        :
        : "xmm0", "memory"
    );

    while (count-- > 0) {
        *d++ = v;
    }

    return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
    char* d = (char*)dest;
    const char* s = (const char*)src;

    if (count < 16) {
        while (count-- > 0) *d++ = *s++;
        return dest;
    }

    __asm__ volatile(
        "1:\n\t"
        "cmp $64, %2\n\t"
        "jl 2f\n\t"
        "movups (%1), %%xmm0\n\t"
        "movups 16(%1), %%xmm1\n\t"
        "movups 32(%1), %%xmm2\n\t"
        "movups 48(%1), %%xmm3\n\t"
        
        "movups %%xmm0, (%0)\n\t"
        "movups %%xmm1, 16(%0)\n\t"
        "movups %%xmm2, 32(%0)\n\t"
        "movups %%xmm3, 48(%0)\n\t"

        "add $64, %1\n\t"
        "add $64, %0\n\t"
        "sub $64, %2\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        : "+r" (d), "+r" (s), "+r" (count)
        :
        : "xmm0", "xmm1", "xmm2", "xmm3", "memory"
    );

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

    nano_print(&buffer[index]);
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

void _float_to_str(double val, char* buf, int precision) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    // Extragem partea întreagă
    int64_t int_part = (int64_t)val;
    double frac_part = val - (double)int_part;

    // Convertim partea întreagă folosind funcția ta existentă
    char int_buf[32];
    _int_to_dec(int_part, int_buf);
    
    // Copiem partea întreagă în bufferul final
    char* p = buf;
    char* src_int = int_buf;
    while (*src_int) {
        *p++ = *src_int++;
    }

    // Adăugăm punctul zecimal dacă precizia > 0
    if (precision > 0) {
        *p++ = '.';

        // Multiplicăm partea fracționară pentru a o transforma în întreg
        for (int i = 0; i < precision; i++) {
            frac_part *= 10.0;
        }
        int64_t frac_int = (int64_t)(frac_part + 0.5); // Rotunjire fină

        // Convertim partea fracționară
        char frac_buf[32];
        _int_to_dec(frac_int, frac_buf);

        // Completăm cu zerouri la stânga dacă e nevoie (ex: .05 în loc de .5)
        int frac_len = string_length(frac_buf);
        for (int i = 0; i < precision - frac_len; i++) {
            *p++ = '0';
        }

        char* src_frac = frac_buf;
        while (*src_frac) {
            *p++ = *src_frac++;
        }
    }
    *p = '\0';
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
			else if (spec == 'f') {
                double val = va_arg(args, double); // În va_arg float devine double
                _float_to_str(val, conv_buf, 6);   // 6 zecimale standard, ca în printf
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


int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            break;
        }
    }
    return 0;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void append_formatted_char(char* buffer, uint32_t* written, char c) {
    if (*written < 1023) buffer[*written] = c;
    if (*written < 1024) (*written)++;
}

int nano_vsnprintf(char* output, size_t output_size, const char* format, va_list args) {
    if (!format || (output_size > 0 && !output)) return -1;

    char buffer[1024];
    uint32_t written = 0;

    while (*format) {
        if (*format != '%') {
            append_formatted_char(buffer, &written, *format++);
            continue;
        }
        format++;
        if (*format == '%') {
            append_formatted_char(buffer, &written, *format++);
            continue;
        }

        int left_align = 0;
        int plus_sign = 0;
        int space_sign = 0;
        int alternate = 0;
        int zero_pad = 0;
        int parsing_flags = 1;
        while (parsing_flags) {
            switch (*format) {
                case '-': left_align = 1; format++; break;
                case '+': plus_sign = 1; format++; break;
                case ' ': space_sign = 1; format++; break;
                case '#': alternate = 1; format++; break;
                case '0': zero_pad = 1; format++; break;
                default: parsing_flags = 0; break;
            }
        }

        int width = 0;
        if (*format == '*') {
            width = va_arg(args, int);
            format++;
            if (width < 0) {
                left_align = 1;
                width = -width;
            }
        } else {
            while (*format >= '0' && *format <= '9') {
                if (width < 1024) width = width * 10 + (*format - '0');
                format++;
            }
        }
        if (width > 1023) width = 1023;

        int precision = -1;
        if (*format == '.') {
            format++;
            precision = 0;
            if (*format == '*') {
                precision = va_arg(args, int);
                format++;
                if (precision < 0) precision = -1;
            } else {
                while (*format >= '0' && *format <= '9') {
                    if (precision < 1024) precision = precision * 10 + (*format - '0');
                    format++;
                }
            }
            if (precision > 1023) precision = 1023;
        }

        enum { LEN_DEFAULT, LEN_LONG, LEN_LLONG, LEN_SIZE, LEN_SHORT, LEN_CHAR } length = LEN_DEFAULT;
        if (*format == 'h') {
            format++;
            if (*format == 'h') {
                length = LEN_CHAR;
                format++;
            } else {
                length = LEN_SHORT;
            }
        } else if (*format == 'l') {
            format++;
            if (*format == 'l') {
                length = LEN_LLONG;
                format++;
            } else {
                length = LEN_LONG;
            }
        } else if (*format == 'z' || *format == 't' || *format == 'j') {
            length = LEN_SIZE;
            format++;
        } else if (*format == 'L') {
            format++;
        }

        char spec = *format;
        if (!spec) {
            append_formatted_char(buffer, &written, '%');
            break;
        }
        format++;

        if (spec == 's') {
            const char* text = va_arg(args, const char*);
            if (!text) text = "(null)";
            uint32_t text_len = 0;
            while (text[text_len] && (precision < 0 || text_len < (uint32_t)precision)) text_len++;
            int padding = width > (int)text_len ? width - (int)text_len : 0;
            if (!left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            for (uint32_t i = 0; i < text_len; i++) append_formatted_char(buffer, &written, text[i]);
            if (left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            continue;
        }

        if (spec == 'c') {
            char c = (char)va_arg(args, int);
            int padding = width > 1 ? width - 1 : 0;
            if (!left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            append_formatted_char(buffer, &written, c);
            if (left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            continue;
        }

        if (spec == 'f' || spec == 'F') {
            char number[64];
            int float_precision = precision < 0 ? 6 : precision;
            if (float_precision > 9) float_precision = 9;
            _float_to_str(va_arg(args, double), number, float_precision);
            uint32_t number_len = 0;
            while (number[number_len] && number_len < sizeof(number) - 1) number_len++;
            int padding = width > (int)number_len ? width - (int)number_len : 0;
            if (!left_align) while (padding-- > 0) append_formatted_char(buffer, &written, zero_pad ? '0' : ' ');
            for (uint32_t i = 0; i < number_len; i++) append_formatted_char(buffer, &written, number[i]);
            if (left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            continue;
        }

        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'o' ||
            spec == 'x' || spec == 'X' || spec == 'p') {
            int is_signed = spec == 'd' || spec == 'i';
            int is_pointer = spec == 'p';
            int is_negative = 0;
            uint64_t value;
            if (is_pointer) {
                value = (uint64_t)va_arg(args, void*);
                alternate = 1;
                spec = 'x';
            } else if (is_signed) {
                int64_t signed_value;
                if (length == LEN_LLONG) signed_value = va_arg(args, long long);
                else if (length == LEN_LONG) signed_value = va_arg(args, long);
                else if (length == LEN_SIZE) signed_value = va_arg(args, int64_t);
                else signed_value = va_arg(args, int);
                if (length == LEN_CHAR) signed_value = (signed char)signed_value;
                else if (length == LEN_SHORT) signed_value = (short)signed_value;
                is_negative = signed_value < 0;
                value = is_negative ? (uint64_t)(-(signed_value + 1)) + 1 : (uint64_t)signed_value;
            } else {
                if (length == LEN_LLONG) value = va_arg(args, unsigned long long);
                else if (length == LEN_LONG) value = va_arg(args, unsigned long);
                else if (length == LEN_SIZE) value = va_arg(args, size_t);
                else value = va_arg(args, unsigned int);
                if (length == LEN_CHAR) value = (unsigned char)value;
                else if (length == LEN_SHORT) value = (unsigned short)value;
            }

            unsigned int base = spec == 'o' ? 8 : ((spec == 'x' || spec == 'X') ? 16 : 10);
            char digits[32];
            char reverse[32];
            const char* alphabet = spec == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            uint32_t digit_count = 0;
            if (value || precision != 0) {
                do {
                    reverse[digit_count++] = alphabet[value % base];
                    value /= base;
                } while (value && digit_count < sizeof(reverse));
            }
            while (digit_count < (uint32_t)(precision > 0 ? precision : 0) && digit_count < sizeof(reverse)) {
                reverse[digit_count++] = '0';
            }
            for (uint32_t i = 0; i < digit_count; i++) digits[i] = reverse[digit_count - i - 1];

            char sign = is_negative ? '-' : (is_signed && plus_sign ? '+' : (is_signed && space_sign ? ' ' : 0));
            char prefix[2];
            uint32_t prefix_len = 0;
            if (alternate && spec == 'o' && (!digit_count || digits[0] != '0')) prefix[prefix_len++] = '0';
            if (alternate && (spec == 'x' || spec == 'X') && digit_count) {
                prefix[prefix_len++] = '0';
                prefix[prefix_len++] = spec;
            }
            uint32_t content_len = digit_count + prefix_len + (sign != 0);
            int padding = width > (int)content_len ? width - (int)content_len : 0;
            int use_zero_padding = zero_pad && !left_align && precision < 0;
            if (!left_align && !use_zero_padding) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            if (sign) append_formatted_char(buffer, &written, sign);
            for (uint32_t i = 0; i < prefix_len; i++) append_formatted_char(buffer, &written, prefix[i]);
            if (use_zero_padding) while (padding-- > 0) append_formatted_char(buffer, &written, '0');
            for (uint32_t i = 0; i < digit_count; i++) append_formatted_char(buffer, &written, digits[i]);
            if (left_align) while (padding-- > 0) append_formatted_char(buffer, &written, ' ');
            continue;
        }

        append_formatted_char(buffer, &written, '%');
        append_formatted_char(buffer, &written, spec);
    }

    buffer[written < 1024 ? written : 1023] = '\0';
    if (output_size > 0) {
        size_t copy_length = written < output_size - 1 ? written : output_size - 1;
        for (size_t i = 0; i < copy_length; i++) output[i] = buffer[i];
        output[copy_length] = '\0';
    }
    return (int)written;
}

int nano_vprintf(const char* format, va_list args) {
    char buffer[1024];
    int result = nano_vsnprintf(buffer, sizeof(buffer), format, args);
    if (result >= 0) nano_print(buffer);
    return result;
}

void nano_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    nano_vprintf(format, args);
    va_end(args);
}