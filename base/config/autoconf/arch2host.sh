# arch2host.sh
# Returns a canonicalized host arch name from a shorthand arch token.
# $1 = Shorthand
# $2 = Default canonical value

case "$1" in
i?86)
    echo $1-pc-linux-gnu
    ;;
x86_64)
    echo x86_64-unknown-linux-gnu
    ;;
ppc|powerpc)
    echo powerpc-unknown-linux-gnu
    ;;
arm)
    echo arm-unknown-linux-gnu
    ;;
m68knommu)
    echo m68knommu-unknown-linux-gnu
    ;;
m68k)
    echo m68k-unknown-linux-gnu
    ;;
"")
    # Shorthand not specified: return default value.
    echo $2
    ;;
*-*)
    # Not a shorthand: return "as is".
    echo $1
    ;;
esac
