#ifndef API_PROXY_H_
#define API_PROXY_H_
// clang-format off
#include <linux/mm_types.h>
#include <asm/uaccess.h>
// clang-format on
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/version.h>

static __always_inline unsigned long x_copy_from_user(void *to, const void __user *from, unsigned long n) {
#ifdef CONFIG_DIRECT_API_USER_COPY
    unsigned long __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
    return __arch_copy_from_user(to, from, n);
#else
    return copy_from_user(to, from, n);
#endif
}
static __always_inline unsigned long x_copy_to_user(void __user *to, const void *from, unsigned long n) {
#ifdef CONFIG_DIRECT_API_USER_COPY
    unsigned long __arch_copy_to_user(void __user *to, const void *from, unsigned long n);
    return __arch_copy_to_user(to, from, n);
    return copy_to_user(to, from, n);
#else
    return copy_to_user(to, from, n);
#endif
}

static __always_inline long x_probe_kernel_read(void *bounce, const char *ptr, size_t sz) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return copy_from_kernel_nofault(bounce, ptr, sz);
#else
    return probe_kernel_read(bounce, ptr, sz);
#endif
}

#endif /* API_PROXY_H_ */
