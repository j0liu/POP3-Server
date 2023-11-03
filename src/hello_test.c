#include <stdio.h>
#include <stdlib.h>
#include <check.h>

#include "hello.h"
#include "tests.h"

#define FIXBUF(b, data) buffer_init(&(b), N(data), (data)); \
                        buffer_write_adv(&(b), N(data))

static void
on_hello_method(struct hello_parser *p, const uint8_t method) {
    uint8_t *selected  = p->data;
    
    if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method
      || method >= 0xFA) {
       *selected = method;
    }
}

START_TEST (test_hello_normal) {
    uint8_t method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
    struct hello_parser parser = {
        .data                     = &method,
        .on_authentication_method = on_hello_method,
    };
    hello_parser_init(&parser);
    uint8_t data[] = {
        0x05, 0x02, 0x00, 0x01,
    };
    buffer b; FIXBUF(b, data);
    bool errored = false;
    enum hello_state st = hello_consume(&b, &parser, &errored);
    ck_assert_uint_eq(false, errored);
    ck_assert_uint_eq(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED, method);
    ck_assert_uint_eq(hello_done, st);
}
END_TEST

START_TEST (test_hello_no_methods) {
    uint8_t method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
    struct hello_parser parser = {
        .data                     = &method,
        .on_authentication_method = &on_hello_method,
    };
    hello_parser_init(&parser);
    
    unsigned char data[] = {
        0x05, 0x00,
    };
    buffer b; FIXBUF(b, data);

    bool errored = false;
    enum hello_state st = hello_consume(&b, &parser, &errored);
    ck_assert_uint_eq(false, errored);
    ck_assert_uint_eq(SOCKS_HELLO_NO_ACCEPTABLE_METHODS, method);
    ck_assert_uint_eq(hello_done, st);
}
END_TEST

START_TEST (test_hello_unsupported_version) {
    struct hello_parser parser = {
        .data                     = 0x00,
        .on_authentication_method = &on_hello_method,
    };
    hello_parser_init(&parser);
    
    unsigned char data[] = {
        0x04,
    };
    buffer b; FIXBUF(b, data);
    bool errored = false;
    enum hello_state st = hello_consume(&b, &parser, &errored);
    ck_assert_uint_eq(hello_error_unsupported_version, st);
    ck_assert_uint_eq(true, errored);
}
END_TEST

START_TEST (test_hello_multiple_request) {
    uint8_t method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
    struct hello_parser parser = {
        .data                     = &method,
        .on_authentication_method = &on_hello_method,
    };
    hello_parser_init(&parser);

    unsigned char data[] = {
        0x05, 0x01, 0xfa,
        0x05, 0x01, 0xfb,
    };
    buffer b; FIXBUF(b, data);
    bool errored = false;
    enum hello_state st;

    st = hello_consume(&b, &parser, &errored);
    ck_assert_uint_eq(false, errored);
    ck_assert_uint_eq(0xfa, method);
    ck_assert_uint_eq(hello_done, st);

    errored = false;
    method = SOCKS_HELLO_NO_ACCEPTABLE_METHODS;
    hello_parser_init(&parser);
    st = hello_consume(&b, &parser, &errored);
    ck_assert_uint_eq(false, errored);
    ck_assert_uint_eq(0xfb, method);
    ck_assert_uint_eq(hello_done, st);
}
END_TEST

Suite * 
hello_suite(void) {
    Suite *s;
    TCase *tc;

    s = suite_create("socks");

    /* Core test case */
    tc = tcase_create("hello");

    tcase_add_test(tc, test_hello_normal);
    tcase_add_test(tc, test_hello_no_methods);
    tcase_add_test(tc, test_hello_unsupported_version);
    tcase_add_test(tc, test_hello_multiple_request);
    suite_add_tcase(s, tc);

    return s;
}

int 
main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = hello_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

