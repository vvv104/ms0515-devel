/*
 * test_main.cpp - doctest entry point with the harness options.
 *
 * Options of the form --saper-<name>=<value> are collected for the tests
 * (saper::options()) and removed before doctest parses the rest.  Paths
 * default to the repo layout.  See SaperGame.hpp for the names.
 */
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include "SaperGame.hpp"

#include <cstring>
#include <vector>

int main(int argc, char **argv)
{
    std::vector<char *> rest;
    rest.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (std::strncmp(a, "--saper-", 8) == 0) {
            const char *eq = std::strchr(a + 8, '=');
            if (eq)
                saper::options()[std::string(a + 8, eq)] = std::string(eq + 1);
            else
                saper::options()[std::string(a + 8)] = "1";
        } else {
            rest.push_back(argv[i]);
        }
    }
    doctest::Context ctx(static_cast<int>(rest.size()), rest.data());
    return ctx.run();
}
