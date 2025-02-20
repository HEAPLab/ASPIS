
// Template function
template <typename T>
T square(T x) {
    return x * x;
}

// Template class
template <typename T>
class TemplateClass {
public:
    TemplateClass(T x, T y) : a(x), b(y) {}
    
    T product() const {
        return a * b;
    }

private:
    T a, b;
};
