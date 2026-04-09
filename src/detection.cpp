#include "detection.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace pw {

namespace {

std::string to_lower(std::string_view s) {
    std::string result(s);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// System apps that should NOT be classified as dev processes
constexpr std::array system_apps = {
    "spotify", "raycast", "tableplus", "postman", "linear", "cursor",
    "controlce", "rapportd", "superhuma", "setappage", "slack", "discord",
    "firefox", "chrome", "google", "safari", "figma", "notion", "zoom",
    "teams", "code", "iterm2", "warp", "arc", "loginwindow", "windowserver",
    "systemuise", "kernel_task", "launchd", "mdworker", "mds_stores",
    "cfprefsd", "coreaudio", "corebrightne", "airportd", "bluetoothd",
    "sharingd", "usernoted", "notificationc", "cloudd"
};

const std::set<std::string> dev_names = {
    "node", "python", "python3", "ruby", "java", "go", "cargo", "deno",
    "bun", "php", "uvicorn", "gunicorn", "flask", "rails", "npm", "npx",
    "yarn", "pnpm", "tsc", "tsx", "esbuild", "rollup", "turbo", "nx",
    "jest", "vitest", "mocha", "pytest", "cypress", "playwright", "rustc",
    "dotnet", "gradle", "mvn", "mix", "elixir"
};

struct CommandPattern {
    std::regex pattern;
    // Not used for matching result, just for is_dev_process detection
};

} // anonymous namespace

std::optional<std::string> detect_framework_from_name(const std::string& process_name) {
    auto name = to_lower(process_name);
    if (name == "node") return "Node.js";
    if (name == "python" || name == "python3") return "Python";
    if (name == "ruby") return "Ruby";
    if (name == "java") return "Java";
    if (name == "go") return "Go";
    return std::nullopt;
}

std::optional<std::string> detect_framework_from_command(const std::string& command, const std::string& process_name) {
    if (command.empty()) return detect_framework_from_name(process_name);

    auto cmd = to_lower(command);

    if (cmd.find("next") != std::string::npos) return "Next.js";
    if (cmd.find("vite") != std::string::npos) return "Vite";
    if (cmd.find("nuxt") != std::string::npos) return "Nuxt";
    if (cmd.find("angular") != std::string::npos || cmd.find("ng serve") != std::string::npos) return "Angular";
    if (cmd.find("webpack") != std::string::npos) return "Webpack";
    if (cmd.find("remix") != std::string::npos) return "Remix";
    if (cmd.find("astro") != std::string::npos) return "Astro";
    if (cmd.find("gatsby") != std::string::npos) return "Gatsby";
    if (cmd.find("flask") != std::string::npos) return "Flask";
    if (cmd.find("django") != std::string::npos || cmd.find("manage.py") != std::string::npos) return "Django";
    if (cmd.find("uvicorn") != std::string::npos) return "FastAPI";
    if (cmd.find("rails") != std::string::npos) return "Rails";
    if (cmd.find("cargo") != std::string::npos || cmd.find("rustc") != std::string::npos) return "Rust";

    return detect_framework_from_name(process_name);
}

std::optional<std::string> detect_framework(const std::string& project_root) {
    // Layer 2: package.json dependency scanning
    auto pkg_path = fs::path(project_root) / "package.json";
    if (fs::exists(pkg_path)) {
        try {
            std::ifstream f(pkg_path);
            auto pkg = nlohmann::json::parse(f);

            // Merge dependencies and devDependencies
            nlohmann::json all_deps;
            if (pkg.contains("dependencies") && pkg["dependencies"].is_object())
                all_deps.merge_patch(pkg["dependencies"]);
            if (pkg.contains("devDependencies") && pkg["devDependencies"].is_object())
                all_deps.merge_patch(pkg["devDependencies"]);

            // Ordered by specificity (most specific first)
            if (all_deps.contains("next")) return "Next.js";
            if (all_deps.contains("nuxt") || all_deps.contains("nuxt3")) return "Nuxt";
            if (all_deps.contains("@sveltejs/kit")) return "SvelteKit";
            if (all_deps.contains("svelte")) return "Svelte";
            if (all_deps.contains("@remix-run/react") || all_deps.contains("remix")) return "Remix";
            if (all_deps.contains("astro")) return "Astro";
            if (all_deps.contains("vite")) return "Vite";
            if (all_deps.contains("@angular/core")) return "Angular";
            if (all_deps.contains("vue")) return "Vue";
            if (all_deps.contains("react")) return "React";
            if (all_deps.contains("express")) return "Express";
            if (all_deps.contains("fastify")) return "Fastify";
            if (all_deps.contains("hono")) return "Hono";
            if (all_deps.contains("koa")) return "Koa";
            if (all_deps.contains("@nestjs/core") || all_deps.contains("nestjs")) return "NestJS";
            if (all_deps.contains("gatsby")) return "Gatsby";
            if (all_deps.contains("webpack-dev-server")) return "Webpack";
            if (all_deps.contains("esbuild")) return "esbuild";
            if (all_deps.contains("parcel")) return "Parcel";
        } catch (...) {}
    }

    // Layer 3: config file detection
    auto root = fs::path(project_root);
    if (fs::exists(root / "vite.config.ts") || fs::exists(root / "vite.config.js")) return "Vite";
    if (fs::exists(root / "next.config.js") || fs::exists(root / "next.config.mjs")) return "Next.js";
    if (fs::exists(root / "angular.json")) return "Angular";
    if (fs::exists(root / "Cargo.toml")) return "Rust";
    if (fs::exists(root / "go.mod")) return "Go";
    if (fs::exists(root / "manage.py")) return "Django";
    if (fs::exists(root / "Gemfile")) return "Ruby";

    return std::nullopt;
}

std::string detect_framework_from_image(const std::string& image) {
    if (image.empty()) return "Docker";
    auto img = to_lower(image);

    if (img.find("postgres") != std::string::npos) return "PostgreSQL";
    if (img.find("redis") != std::string::npos) return "Redis";
    if (img.find("mysql") != std::string::npos || img.find("mariadb") != std::string::npos) return "MySQL";
    if (img.find("mongo") != std::string::npos) return "MongoDB";
    if (img.find("nginx") != std::string::npos) return "nginx";
    if (img.find("localstack") != std::string::npos) return "LocalStack";
    if (img.find("rabbitmq") != std::string::npos) return "RabbitMQ";
    if (img.find("kafka") != std::string::npos) return "Kafka";
    if (img.find("elasticsearch") != std::string::npos || img.find("opensearch") != std::string::npos) return "Elasticsearch";
    if (img.find("minio") != std::string::npos) return "MinIO";

    return "Docker";
}

bool is_dev_process(const std::string& process_name, const std::string& command) {
    auto name = to_lower(process_name);
    auto cmd = to_lower(command);

    // Check system app blacklist (prefix match)
    for (const auto& app : system_apps) {
        if (name.find(app) == 0) return false;
    }

    // Check dev process whitelist (exact match)
    if (dev_names.count(name)) return true;

    // Docker processes
    if (name.find("com.docke") == 0 || name == "docker" || name == "docker-sandbox") return true;

    // Command-line keyword matching
    static const std::vector<std::regex> cmd_indicators = {
        std::regex(R"(\bnode\b)"),
        std::regex(R"(\bnext[\s-])"),
        std::regex(R"(\bvite\b)"),
        std::regex(R"(\bnuxt\b)"),
        std::regex(R"(\bwebpack\b)"),
        std::regex(R"(\bremix\b)"),
        std::regex(R"(\bastro\b)"),
        std::regex(R"(\bgulp\b)"),
        std::regex(R"(\bng serve\b)"),
        std::regex(R"(\bgatsb)"),
        std::regex(R"(\bflask\b)"),
        std::regex(R"(\bdjango\b|manage\.py)"),
        std::regex(R"(\buvicorn\b)"),
        std::regex(R"(\brails\b)"),
        std::regex(R"(\bcargo\b)"),
    };

    for (const auto& re : cmd_indicators) {
        if (std::regex_search(cmd, re)) return true;
    }

    return false;
}

} // namespace pw
