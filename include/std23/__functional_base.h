#ifndef INCLUDE_STD23____FUNCTIONAL__BASE
#define INCLUDE_STD23____FUNCTIONAL__BASE

#include <functional>
#include <utility>

namespace std23
{

template<auto V> struct nontype_t
{
    explicit nontype_t() = default;
};

template<auto V> inline constexpr nontype_t<V> nontype{};

template<class R, class F, class... Args>
requires std::is_invocable_r_v<R, F, Args...>
constexpr R invoke_r(F &&f, Args &&...args) noexcept(
    std::is_nothrow_invocable_r_v<R, F, Args...>)
{
    if constexpr (std::is_void_v<R>)
        std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    else
        return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

// See also: https://www.agner.org/optimize/calling_conventions.pdf
template<class T>
inline constexpr auto _select_param_type = []
{
    if constexpr (std::is_trivially_copyable_v<T>)
        return std::type_identity<T>();
    else
        return std::add_rvalue_reference<T>();
};

template<class T>
using _param_t =
    typename std::invoke_result_t<decltype(_select_param_type<T>)>::type;

template<class T, class Self>
inline constexpr bool _is_not_self =
    not std::is_same_v<std::remove_cvref_t<T>, Self>;

template<class T, template<class...> class>
inline constexpr bool _looks_nullable_to_impl = std::is_member_pointer_v<T>;

template<class F, template<class...> class Self>
inline constexpr bool _looks_nullable_to_impl<F *, Self> =
    std::is_function_v<F>;

template<class... S, template<class...> class Self>
inline constexpr bool _looks_nullable_to_impl<Self<S...>, Self> = true;

template<class S, template<class...> class Self>
inline constexpr bool _looks_nullable_to =
    _looks_nullable_to_impl<std::remove_cvref_t<S>, Self>;

template<class T> struct _adapt_signature;

template<class F>
requires std::is_function_v<F>
struct _adapt_signature<F *>
{
    using type = F;
};

template<class Fp>
using _adapt_signature_t = typename _adapt_signature<Fp>::type;

template<class S> struct _not_qualifying_this
{};

template<class R, class... Args> struct _not_qualifying_this<R(Args...)>
{
    using type = R(Args...);
};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const> : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) volatile>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const volatile>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) &> : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const &>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) volatile &>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const volatile &>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) &&> : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const &&>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) volatile &&>
    : _not_qualifying_this<R(Args...)>
{};

template<class R, class... Args>
struct _not_qualifying_this<R(Args...) const volatile &&>
    : _not_qualifying_this<R(Args...)>
{};

template<class T> struct _drop_first_arg_to_invoke;

template<class R, class T, class... Args>
struct _drop_first_arg_to_invoke<R (*)(T, Args...)>
{
    using type = R(Args...);
};

template<class R, class T, class... Args>
struct _drop_first_arg_to_invoke<R (*)(T, Args...) noexcept>
{
    using type = R(Args...) noexcept;
};

template<class T, class Cls>
requires std::is_object_v<T>
struct _drop_first_arg_to_invoke<T Cls::*>
{
    using type = T();
};

template<class T, class Cls>
requires std::is_function_v<T>
struct _drop_first_arg_to_invoke<T Cls::*> : _not_qualifying_this<T>
{};

template<class Fp>
using _drop_first_arg_to_invoke_t =
    typename _drop_first_arg_to_invoke<Fp>::type;

} // namespace std23

#endif
