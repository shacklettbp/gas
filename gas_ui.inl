namespace gas {

inline WindowState & operator|=(WindowState &a, WindowState b)
{
    a = WindowState(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline WindowState operator|(WindowState a, WindowState b)
{
    a |= b;

    return a;
}

inline WindowState & operator&=(WindowState &a, WindowState b)
{
    a = WindowState(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline WindowState operator&(WindowState a, WindowState b)
{
    a &= b;

    return a;
}

inline WindowInitFlags & operator|=(WindowInitFlags &a, WindowInitFlags b)
{
    a = WindowInitFlags(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    return a;
}

inline WindowInitFlags operator|(WindowInitFlags a, WindowInitFlags b)
{
    a |= b;

    return a;
}

inline WindowInitFlags & operator&=(WindowInitFlags &a, WindowInitFlags b)
{
    a = WindowInitFlags(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline WindowInitFlags operator&(WindowInitFlags a, WindowInitFlags b)
{
    a &= b;

    return a;
}

}
