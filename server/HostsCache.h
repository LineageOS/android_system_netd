/*
 * Copyright (C) 2016 Cyanogen, Inc.
 */

#ifndef _HOSTSCACHE_H__
#define _HOSTSCACHE_H__

#include <vector>

struct cache_entry {
    uint32_t    addr;
    uint32_t    name;
};

class HostsCache {
public:
    HostsCache();
    virtual ~HostsCache();

    uint32_t getAddrInfo(const char* host, const char* service,
                         const struct addrinfo* hints,
                         struct addrinfo** result);

    struct hostent* getHostByName(const char* host, int af);

private:
    void initCache();
    void mapHostsFile();

    struct cache_entry* findName(const char* name);

private:
    int                         mHostsFd;
    off_t                       mHostsSize;
    long                        mHostsModTime;
    char*                       mHostsData;
    std::vector<cache_entry>    mCache;
};

#endif
