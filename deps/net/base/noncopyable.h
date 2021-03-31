#pragma once

namespace skynet {

class noncopyable
{
protected:
    constexpr noncopyable() = default;
    ~noncopyable() = default;

protected:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};

}


