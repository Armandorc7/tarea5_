/*
Tarea #5: Patrones de Diseño
armando ruesta
*/

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <cmath>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>

/* ------------------ Ejercicio 1: Singleton ------------------ */

class Account {
    std::string id_;
    double balance_;
    mutable std::mutex mtx_;
public:
    Account(const std::string& id, double balance) : id_(id), balance_(balance) {}
    void deposit(double amount) { std::lock_guard<std::mutex> lock(mtx_); balance_ += amount; }
    bool withdraw(double amount) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (balance_ < amount) return false;
        balance_ -= amount;
        return true;
    }
    double getBalance() const { std::lock_guard<std::mutex> lock(mtx_); return balance_; }
    const std::string& getId() const { return id_; }
};

enum class TransactionType { DEPOSIT, WITHDRAW };

struct Transaction {
    TransactionType type;
    std::string accountId;
    double amount;
    Transaction(TransactionType t, const std::string& id, double amt)
        : type(t), accountId(id), amount(amt) {}
};

class TransactionManager {
    std::vector<Transaction> log_;
    mutable std::mutex logMtx_;
    TransactionManager() = default;
public:
    TransactionManager(const TransactionManager&) = delete;
    TransactionManager& operator=(const TransactionManager&) = delete;

    static TransactionManager& getInstance() {
        static TransactionManager instance;
        return instance;
    }

    void deposit(Account* acc, double amount) {
        acc->deposit(amount);
        std::lock_guard<std::mutex> lock(logMtx_);
        log_.emplace_back(TransactionType::DEPOSIT, acc->getId(), amount);
    }

    bool withdraw(Account* acc, double amount) {
        bool ok = acc->withdraw(amount);
        std::lock_guard<std::mutex> lock(logMtx_);
        if (ok) log_.emplace_back(TransactionType::WITHDRAW, acc->getId(), amount);
        return ok;
    }
};

void runEj1() {
    std::cout << "\nEj1\n";
    auto& manager = TransactionManager::getInstance();
    Account account("A1", 1000);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            manager.deposit(&account, 100);
            manager.withdraw(&account, 50);
        });
    }
    for (auto& t : threads) t.join();

    // Cada hilo hace deposit(100) y withdraw(50) => neto +50 por hilo
    // 1000 + 10*100 - 10*50 = 1500
    std::cout << "Balance final: " << account.getBalance() << "\n";
    std::cout << "Direccion manager: " << &TransactionManager::getInstance() << "\n";
}

/* ------------------ Ejercicio 2: Abstract Factory ------------------ */

class Window { public: virtual void draw() const = 0; virtual ~Window() = default; };
class Button { public: virtual void draw() const = 0; virtual ~Button() = default; };
class Shader { public: virtual void compile() const = 0; virtual ~Shader() = default; };

class OpenGLWindow : public Window { public: void draw() const override { std::cout << "[OpenGL] Window\n"; } };
class OpenGLButton : public Button { public: void draw() const override { std::cout << "[OpenGL] Button\n"; } };
class OpenGLShader : public Shader { public: void compile() const override { std::cout << "[OpenGL] Shader\n"; } };

class VulkanWindow : public Window { public: void draw() const override { std::cout << "[Vulkan] Window\n"; } };
class VulkanButton : public Button { public: void draw() const override { std::cout << "[Vulkan] Button\n"; } };
class VulkanShader : public Shader { public: void compile() const override { std::cout << "[Vulkan] Shader\n"; } };

class RenderFactory {
public:
    virtual std::unique_ptr<Window> createWindow() const = 0;
    virtual std::unique_ptr<Button> createButton() const = 0;
    virtual std::unique_ptr<Shader> createShader() const = 0;
    virtual ~RenderFactory() = default;
};

class OpenGLFactory : public RenderFactory {
public:
    std::unique_ptr<Window> createWindow() const override { return std::make_unique<OpenGLWindow>(); }
    std::unique_ptr<Button> createButton() const override { return std::make_unique<OpenGLButton>(); }
    std::unique_ptr<Shader> createShader() const override { return std::make_unique<OpenGLShader>(); }
};

class VulkanFactory : public RenderFactory {
public:
    std::unique_ptr<Window> createWindow() const override { return std::make_unique<VulkanWindow>(); }
    std::unique_ptr<Button> createButton() const override { return std::make_unique<VulkanButton>(); }
    std::unique_ptr<Shader> createShader() const override { return std::make_unique<VulkanShader>(); }
};

void runEj2() {
    std::cout << "\nEj2\n";
    std::unique_ptr<RenderFactory> factory = std::make_unique<OpenGLFactory>();
    factory->createWindow()->draw();
    factory->createButton()->draw();

    factory = std::make_unique<VulkanFactory>();
    factory->createShader()->compile();
    factory->createWindow()->draw();
}

/* ------------------ Ejercicio 3: Composite ------------------ */

class Expression {
public:
    virtual double evaluate(double x) const = 0;
    virtual std::shared_ptr<Expression> derivative() const = 0;
    virtual std::string toString() const = 0;
    virtual bool isConstantZero() const { return false; }
    virtual bool isConstantOne()  const { return false; }
    virtual ~Expression() = default;
};
using Expr = std::shared_ptr<Expression>;

static bool isZero(const Expr& e) { return e->isConstantZero(); }
static bool isOne (const Expr& e) { return e->isConstantOne();  }

class Constant : public Expression {
    double val_;
public:
    explicit Constant(double v) : val_(v) {}
    double evaluate(double) const override { return val_; }
    bool isConstantZero() const override { return val_ == 0.0; }
    bool isConstantOne()  const override { return val_ == 1.0; }
    Expr derivative() const override { return std::make_shared<Constant>(0.0); }
    std::string toString() const override {
        std::ostringstream oss;
        if (val_ == std::floor(val_)) oss << static_cast<long long>(val_);
        else oss << val_;
        return oss.str();
    }
};

class Variable : public Expression {
public:
    double evaluate(double x) const override { return x; }
    Expr derivative() const override { return std::make_shared<Constant>(1.0); }
    std::string toString() const override { return "x"; }
};

class Add : public Expression {
    Expr l_, r_;
public:
    Add(Expr l, Expr r) : l_(l), r_(r) {}
    double evaluate(double x) const override { return l_->evaluate(x) + r_->evaluate(x); }
    Expr derivative() const override {
        auto dl = l_->derivative(), dr = r_->derivative();
        if (isZero(dl)) return dr;
        if (isZero(dr)) return dl;
        return std::make_shared<Add>(dl, dr);
    }
    std::string toString() const override { return "(" + l_->toString() + " + " + r_->toString() + ")"; }
};

class Multiply : public Expression {
    Expr l_, r_;
public:
    Multiply(Expr l, Expr r) : l_(l), r_(r) {}
    double evaluate(double x) const override { return l_->evaluate(x) * r_->evaluate(x); }
    Expr derivative() const override {
        auto dl = l_->derivative(), dr = r_->derivative();
        Expr t1 = isZero(dl) ? std::make_shared<Constant>(0.0) : (isOne(dl) ? r_ : std::make_shared<Multiply>(dl, r_));
        Expr t2 = isZero(dr) ? std::make_shared<Constant>(0.0) : (isOne(dr) ? l_ : std::make_shared<Multiply>(l_, dr));
        if (isZero(t1)) return t2;
        if (isZero(t2)) return t1;
        return std::make_shared<Add>(t1, t2);
    }
    std::string toString() const override { return "(" + l_->toString() + " * " + r_->toString() + ")"; }
};

class Power : public Expression {
    Expr base_, exp_;
public:
    Power(Expr base, Expr exp) : base_(base), exp_(exp) {}
    double evaluate(double x) const override { return std::pow(base_->evaluate(x), exp_->evaluate(x)); }
    Expr derivative() const override {
        double n = exp_->evaluate(0);
        if (n == 0) return std::make_shared<Constant>(0.0);
        auto inner = std::make_shared<Multiply>(
            std::make_shared<Constant>(n),
            std::make_shared<Power>(base_, std::make_shared<Constant>(n - 1))
        );
        auto bd = base_->derivative();
        return isOne(bd) ? inner : std::make_shared<Multiply>(inner, bd);
    }
    std::string toString() const override { return "(" + base_->toString() + "^" + exp_->toString() + ")"; }
};

class Cos;

class Sin : public Expression {
    Expr arg_;
public:
    explicit Sin(Expr arg) : arg_(arg) {}
    double evaluate(double x) const override { return std::sin(arg_->evaluate(x)); }
    Expr derivative() const override;
    std::string toString() const override { return "sin(" + arg_->toString() + ")"; }
};

class Cos : public Expression {
    Expr arg_;
public:
    explicit Cos(Expr arg) : arg_(arg) {}
    double evaluate(double x) const override { return std::cos(arg_->evaluate(x)); }
    Expr derivative() const override {
        auto outer = std::make_shared<Multiply>(
            std::make_shared<Constant>(-1.0),
            std::make_shared<Sin>(arg_)
        );
        auto ad = arg_->derivative();
        return isOne(ad) ? outer : std::make_shared<Multiply>(outer, ad);
    }
    std::string toString() const override { return "cos(" + arg_->toString() + ")"; }
};

Expr Sin::derivative() const {
    auto cosU = std::make_shared<Cos>(arg_);
    auto ad = arg_->derivative();
    if (isOne(ad)) return cosU;
    return std::make_shared<Multiply>(cosU, ad);
}

void runEj3() {
    std::cout << "\nEj3\n";
    auto x = std::make_shared<Variable>();
    auto expr = std::make_shared<Multiply>(
        std::make_shared<Add>(
            std::make_shared<Power>(x, std::make_shared<Constant>(2)),
            std::make_shared<Multiply>(std::make_shared<Constant>(3), x)
        ),
        std::make_shared<Sin>(x)
    );
    auto deriv = expr->derivative();
    std::cout << "f(x)  = " << expr->toString()  << "\n";
    std::cout << "f'(x) = " << deriv->toString() << "\n";
    std::cout << "f(2)  = " << expr->evaluate(2) << "\n";
    std::cout << "f'(2) = " << deriv->evaluate(2) << "\n";
}

/* ------------------ Ejercicio 4: Iterator ------------------ */

struct Node {
    int id;
    explicit Node(int i) : id(i) {}
};

class Graph {
    std::unordered_map<int, std::vector<int>> adj_;
public:
    void addEdge(int from, int to) {
        adj_[from].push_back(to);
        if (!adj_.count(to)) adj_[to] = {};
    }
    const std::vector<int>& neighbors(int id) const {
        static const std::vector<int> empty;
        auto it = adj_.find(id);
        return it != adj_.end() ? it->second : empty;
    }
};

class GraphIterator {
public:
    virtual bool hasNext() const = 0;
    virtual int  next() = 0;
    virtual ~GraphIterator() = default;
};

class BFSIterator : public GraphIterator {
    const Graph& g_;
    std::queue<int> q_;
    std::unordered_set<int> visited_;
public:
    BFSIterator(const Graph& g, int start) : g_(g) { q_.push(start); visited_.insert(start); }
    bool hasNext() const override { return !q_.empty(); }
    int next() override {
        int cur = q_.front(); q_.pop();
        for (int nb : g_.neighbors(cur))
            if (!visited_.count(nb)) { visited_.insert(nb); q_.push(nb); }
        return cur;
    }
};

class DFSIterator : public GraphIterator {
    const Graph& g_;
    std::stack<int> s_;
    std::unordered_set<int> visited_;
public:
    DFSIterator(const Graph& g, int start) : g_(g) { s_.push(start); }
    bool hasNext() const override { return !s_.empty(); }
    int next() override {
        while (!s_.empty()) {
            int cur = s_.top(); s_.pop();
            if (visited_.count(cur)) continue;
            visited_.insert(cur);
            const auto& nbrs = g_.neighbors(cur);
            for (auto it = nbrs.rbegin(); it != nbrs.rend(); ++it)
                if (!visited_.count(*it)) s_.push(*it);
            return cur;
        }
        return -1;
    }
};

void runEj4() {
    std::cout << "\nEj4\n";
    Graph g;
    g.addEdge(1, 2); g.addEdge(1, 3); g.addEdge(2, 4); g.addEdge(3, 4); g.addEdge(4, 5);

    std::cout << "BFS: ";
    std::unique_ptr<GraphIterator> it = std::make_unique<BFSIterator>(g, 1);
    while (it->hasNext()) std::cout << it->next() << " ";
    std::cout << "\n";

    std::cout << "DFS: ";
    it = std::make_unique<DFSIterator>(g, 1);
    while (it->hasNext()) std::cout << it->next() << " ";
    std::cout << "\n";
}

int main() {
    runEj1();
    runEj2();
    runEj3();
    runEj4();
    return 0;
}