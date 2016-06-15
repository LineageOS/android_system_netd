/*
 * Copyright (C) 2016 Cyanogen, Inc.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define LOG_TAG "HostsCache"
#include <cutils/log.h>

#include "HostsCache.h"

#define MAX_ADDRLEN 64
#define MAX_HOSTLEN 256

static struct hostent gHostEnt;

static size_t hstrlen(const char *s)
{
    const char *p = s;
    while (*p && !isspace(*p))
        ++p;
    return p - s;
}

static int hstrcmp(const char *a, const char *b)
{
    size_t alen = hstrlen(a);
    size_t blen = hstrlen(b);
    int res = (alen < blen) ? strncmp(a, b, alen) : strncmp(a, b, blen);
    if (res == 0)
        res = alen - blen;
    return res;
}

#if 0
static char *hstrcpy(char *dest, const char *src)
{
    size_t len = hstrlen(src);
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}
#endif

static char *hstrdup(const char *s)
{
    size_t len = hstrlen(s);
    char *dest = (char *)malloc(len + 1);
    memcpy(dest, s, len);
    dest[len] = '\0';
    return dest;
}

class CacheEntryComparator {
public:
    CacheEntryComparator(const char* data) : mData(data) {}
    bool operator()(const struct cache_entry& lhs, const struct cache_entry& rhs) {
        const char* lhsname = mData + lhs.name;
        const char* rhsname = mData + rhs.name;

        return (hstrcmp(lhsname, rhsname) < 0);
    }
private:
    const char* mData;
};

#if 0
static bool cmp_htent_name(const struct cache_entry& lhs, const struct cache_entry& rhs)
{
    const char* lhsname = mHostsData + lhs.name;
    const char* rhsname = mHostsData + rhs.name;

    return (hstrcmp(lhsname, rhsname) < 0);
}
#endif

struct cache_entry* HostsCache::findName(const char *name)
{
    size_t first, last, mid;

    if (mCache.empty())
        return NULL;

    first = 0;
    last = mCache.size();
    mid = (first + last) / 2;
    while (first <= last) {
        struct cache_entry& cur = mCache[mid];
        int cmp = hstrcmp(mHostsData + cur.name, name);
        if (cmp == 0)
            return &cur;
        if (cmp < 0)
            first = mid + 1;
        else
            last = mid - 1;
        mid = (first + last) / 2;
    }
    return NULL;
}

HostsCache::HostsCache() :
        mHostsFd(-1),
        mHostsData(NULL) {}

HostsCache::~HostsCache() {
    if (!mCache.empty()) {
        munmap(mHostsData, mHostsSize);
        mHostsData = NULL;
        close(mHostsFd);
        mHostsFd = -1;
    }
}

uint32_t HostsCache::getAddrInfo(const char* host, const char* service,
                                 const struct addrinfo* hints,
                                 struct addrinfo** result) {
    ALOGI("getAddrInfo: %s,%s\n",
            (host ? host : "(null)"),
            (service ? service : "(null)"));

    uint32_t rc = EAI_NONAME;

    initCache();

    struct cache_entry* ent = findName(host);
    if (!ent) {
        ALOGI("getAddrInfo: not found\n");
        return EAI_NONAME;
    }

    char* addr = hstrdup(mHostsData + ent->addr);
    ALOGI("getAddrInfo: addr=%s\n", addr);

    int family = hints ? hints->ai_family : PF_UNSPEC;
    int socktype = hints ? hints->ai_socktype : 0;
    int protocol = hints ? hints->ai_protocol : 0;

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;

    struct addrinfo* ai;
    ai = (struct addrinfo*)malloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_storage));
    ai->ai_addr = (struct sockaddr*)(ai + 1);

    // Port
    unsigned long port = 0;
    if (service) {
        char* endptr;
        port = strtoul(service, &endptr, 10);
        if (port > 0xffff || *endptr != '\0') {
            port = 0;
        }
        if (!port) {
            const char* proto = NULL;
            switch (socktype) {
            case SOCK_DGRAM:  proto = "udp"; break;
            case SOCK_STREAM: proto = "tcp"; break;
            }
            struct servent* se;
            se = getservbyname(service, proto);
            if (se) {
                port = se->s_port;
            }
        }
    }

    // Address
    if (inet_pton(AF_INET, addr, &sa4.sin_addr) == 1) {
        ALOGI("getAddrInfo: IPv4\n");
        if (family != PF_UNSPEC && family != PF_INET) {
            ALOGI("getAddrInfo: family fail\n");
            goto err;
        }
        family = PF_INET;
        sa4.sin_family = AF_INET;
        sa4.sin_port = htons(port);
        ai->ai_addrlen = sizeof(sa4);
        memcpy(ai->ai_addr, &sa4, sizeof(sa4));
    }
    else if (inet_pton(AF_INET6, addr, &sa6.sin6_addr) == 1) {
        ALOGI("getAddrInfo: IPv6\n");
        if (family != PF_UNSPEC && family != PF_INET6) {
            ALOGI("getAddrInfo: family fail\n");
            goto err;
        }
        family = PF_INET6;
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = htons(port);
        ai->ai_addrlen = sizeof(sa6);
        memcpy(ai->ai_addr, &sa6, sizeof(sa6));
    }
    else {
        ALOGI("getAddrInfo: inet_pton fail\n");
        goto err;
    }

    ai->ai_flags = 0;
    ai->ai_family = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_canonname = NULL;
    ai->ai_next = NULL;
    rc = 0;

out:
    free(addr);
    *result = ai;

    return rc;

err:
    free(ai);
    ai = NULL;
    goto out;
}

struct hostent* HostsCache::getHostByName(const char* host, int af) {
    ALOGI("getHostByName: %s\n",
            (host ? host : "(null)"));

    initCache();

    free(gHostEnt.h_addr_list);
    gHostEnt.h_addr_list = NULL;
    gHostEnt.h_length = 0;
    free(gHostEnt.h_name);
    gHostEnt.h_name = NULL;

    struct cache_entry* ent = findName(host);
    if (!ent) {
        ALOGI("getHostByName: not found\n");
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    char* name = hstrdup(mHostsData + ent->name);

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;

    if (inet_pton(AF_INET, name, &sa4.sin_addr) == 1) {
        if (af != PF_UNSPEC && af != PF_INET) {
            goto err;
        }
        ALOGI("getHostByName: IPv4\n");
        gHostEnt.h_addrtype = AF_INET;
        gHostEnt.h_length = 1;
        char* buf = (char*)malloc(2 * sizeof(char*) + sizeof(struct in_addr));
        struct in_addr* addr0 = (struct in_addr*)(buf + 2 * sizeof(char*));
        memcpy(addr0, &sa4.sin_addr, sizeof(struct in_addr));
        gHostEnt.h_addr_list[0] = (char*)addr0;
        gHostEnt.h_addr_list[1] = NULL;
    }
    else if (inet_pton(AF_INET6, name, &sa6.sin6_addr) == 1) {
        if (af != PF_UNSPEC && af != PF_INET6) {
            goto err;
        }
        ALOGI("getHostByName: IPv6\n");
        gHostEnt.h_addrtype = AF_INET6;
        char* buf = (char*)malloc(2 * sizeof(char*) + sizeof(struct in6_addr));
        struct in6_addr* addr0 = (struct in6_addr*)(buf + 2 * sizeof(char*));
        memcpy(addr0, &sa6.sin6_addr, sizeof(struct in6_addr));
        gHostEnt.h_addr_list[0] = (char*)addr0;
        gHostEnt.h_addr_list[1] = NULL;
    }
    else {
        goto err;
    }

    gHostEnt.h_name = name;

    return &gHostEnt;

err:
    free(name);
    h_errno = HOST_NOT_FOUND;
    return NULL;
}

void HostsCache::initCache() {
    mapHostsFile();
}

void HostsCache::mapHostsFile() {
    int h_fd;
    struct stat st;

    h_fd = open(_PATH_HOSTS, O_RDONLY);
    if (h_fd < 0)
        return;
    if (flock(h_fd, LOCK_EX) != 0) { /* XXX: EINTR */
        close(h_fd);
        return;
    }

    if (!mCache.empty()) {
        memset(&st, 0, sizeof(st));
        if (fstat(h_fd, &st) == 0) {
            if (st.st_size == mHostsSize &&
                st.st_mtime == mHostsModTime) {
                flock(h_fd, LOCK_UN);
                close(h_fd);
                return;
            }
        }
        mCache.clear();
        munmap(mHostsData, mHostsSize);
        close(mHostsFd);
        mHostsFd = -1;
    }

    if (fstat(h_fd, &st) != 0) {
        flock(h_fd, LOCK_UN);
        close(h_fd);
        return;
    }
    char* h_addr;
    h_addr = (char*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, h_fd, 0);
    if (h_addr == MAP_FAILED) {
        flock(h_fd, LOCK_UN);
        close(h_fd);
        return;
    }

    mHostsFd = h_fd;
    mHostsSize = st.st_size;
    mHostsModTime = st.st_mtime;
    mHostsData = h_addr;

    const char* p = (const char *)h_addr;
    const char* pend = p + st.st_size;
    while (p < pend) {
        const char *eol, *addr, *name;
        addr = p;
        eol = (const char*)memchr(p, '\n', pend - p);
        if (!eol)
            break;
        p = eol + 1;
        if (*addr == '#' || *addr == '\n')
            continue;
        if (hstrlen(addr) > MAX_ADDRLEN)
            continue;
        name = addr;
        while (name < eol && !isspace(*name))
            ++name;
        while (name < eol && isspace(*name))
            ++name;
        while (name < eol) {
            size_t namelen = hstrlen(name);
            if (namelen < MAX_HOSTLEN) {
                struct cache_entry ent;
                ent.addr = addr - h_addr;
                ent.name = name - h_addr;
                mCache.push_back(ent);
            }
            name += namelen;
            while (name < eol && isspace(*name))
                ++name;
        }
    }

    CacheEntryComparator cmp(mHostsData);
    std::sort(mCache.begin(), mCache.end(), cmp);

    flock(h_fd, LOCK_UN);
}
