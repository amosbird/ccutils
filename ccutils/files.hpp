#pragma once

#include <climits>
#include <dirent.h>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace ccutils {

using String = std::string;
using ErrorCode = int;

class BaseException {
public:
    virtual ~BaseException() {}
};

class Exception : public BaseException, public std::exception {
public:
    Exception(ErrorCode code)
        : code_(code) {}

private:
    ErrorCode code_;
};

class RuntimeError : public BaseException, public std::runtime_error {
public:
    RuntimeError()
        : std::runtime_error(String{}) {}
    RuntimeError(const char* message)
        : std::runtime_error(message) {}
    RuntimeError(String message)
        : std::runtime_error(message) {}
};

#define CREFILE_EXCEPTION_BASE(name)                                                               \
public:                                                                                            \
    name(ErrorCode code)                                                                           \
        : Exception(code) {}

class NotImplementedException : public RuntimeError {};

class FileExistsException : public Exception {
    CREFILE_EXCEPTION_BASE(FileExistsException);
};
class NoSuchFileException : public Exception {
    CREFILE_EXCEPTION_BASE(NoSuchFileException);
};
class NotDirectoryException : public Exception {
    CREFILE_EXCEPTION_BASE(NotDirectoryException);
};
class NoPermissionException : public Exception {
    CREFILE_EXCEPTION_BASE(NoPermissionException);
};
class UnknownErrorException : public Exception {
    CREFILE_EXCEPTION_BASE(UnknownErrorException);
};

#undef CREFILE_EXCEPTION_BASE

namespace {

    static bool is_slash(const char c) { return c == '/' || c == '\\'; }

    class WinPolicy {
    public:
        static const char Separator = '\\';
    };

    class PosixPolicy {
    public:
        static const char Separator = '/';
    };

    std::vector<String> split_impl(const char* base, size_t size) {
        std::vector<String> res;
        size_t start_pos = 0;
        for (size_t i = 0; i < size; ++i) {
            if (is_slash(base[i])) {
                res.push_back(String{ base + start_pos, base + i + 1 });
                start_pos = i + 1;
            }
        }

        if (size - start_pos > 0) {
            res.push_back(String{ base + start_pos, base + size });
        }

        return res;
    }

    void check_error(ErrorCode code) {
        if (code != 0) {
            const auto error = errno;
            switch (error) {
            case EPERM:
            case EACCES:
                throw NoPermissionException{ error };
            case ENOENT:
                throw NoSuchFileException{ error };
            case EEXIST:
                throw FileExistsException{ error };
            case ENOTDIR:
                throw NotDirectoryException{ error };
            default:
                throw UnknownErrorException{ error };
            }
        }
    }
}

String dirname(const String& filename) {
    auto last_slash = filename.find_last_of('/');
    if (last_slash == String::npos) {
        last_slash = filename.find_last_of("\\\\");
        if (last_slash == String::npos) {
            return filename;
        }
    }
    return filename.substr(0, last_slash);
}

String extension(const String& filename) {
    const auto last_dot = filename.find_last_of('.');
    if (last_dot == String::npos) {
        return String{};
    } else {
        return filename.substr(last_dot + 1);
    }
}

std::vector<String> split(const String& path) { return split_impl(path.c_str(), path.size()); }

std::vector<String> split(const char* base, size_t size) { return split_impl(base, size); }

class PosixPath {
private:
    using Policy = PosixPolicy;
    static void path_join_append_one(String& to, const char* append) {
        if (!to.empty() && !is_slash(to[to.size() - 1])) {
            to += Policy::Separator;
        }
        to += append;
    }

    static void path_join_append_one(String& to, const PosixPath& append) {
        if (!to.empty() && !is_slash(to[to.size() - 1])) {
            to += Policy::Separator;
        }
        to += append.str();
    }

    template <typename First> static void path_join_impl(String& buf, First first) {
        path_join_append_one(buf, first);
    }

    template <typename First, typename... Types>
    static void path_join_impl(String& buf, First first, Types... tail) {
        path_join_append_one(buf, first);
        path_join_impl(buf, tail...);
    }

public:
    PosixPath() {}
    PosixPath(const char* path)
        : path_(path) {}
    PosixPath(String path)
        : path_(std::move(path)) {}

    template <typename... Types> PosixPath(Types... args) { path_join_impl(path_, args...); }

    const String& str() const { return path_; }
    const char* c_str() const { return path_.c_str(); }

    template <typename... Types> static PosixPath join(Types... args) {
        String buf;
        path_join_impl(buf, args...);
        return PosixPath{ buf };
    }

    String dirname() const { return ccutils::dirname(path_); }

    String extension() const { return ccutils::extension(path_); }

    std::vector<String> split() const { return ccutils::split(path_); }

    bool is_abspath() const { return PosixPath::is_abspath(path_); }

    static bool is_abspath(const String& path) {
        if (path.empty()) {
            return false;
        }
        return path[0] == Policy::Separator;
    }

protected:
    String str_move() { return std::move(path_); }

private:
    String path_;
};

bool operator==(const PosixPath& a, const PosixPath& b) { return a.str() == b.str(); }

bool operator!=(const PosixPath& a, const PosixPath& b) { return a.str() != b.str(); }

bool operator<(const PosixPath& a, const PosixPath& b) { return a.str() < b.str(); }

class FileInfoImplUnix {
private:
    void valid() const {
        if (is_end()) {
            throw RuntimeError("Can't get info from invalid file!");
        }
    }

    struct stat* get_stat() const {
        valid();
        if (!stat_) {
            stat_ = std::make_shared<struct stat>();
            const auto path = PosixPath(from_dir_, entry_->d_name);
            const auto res = lstat(path.c_str(), stat_.get());
            check_error(res);
        }
        return stat_.get();
    }

public:
    FileInfoImplUnix() = default;

    FileInfoImplUnix(dirent* entry, PosixPath from_dir)
        : entry_(entry)
        , from_dir_(from_dir) {}

    dirent* native_ptr_impl() { return entry_; }
    const dirent* native_ptr_impl() const { return entry_; }

    String name() const {
        if (is_end()) {
            return String{};
        } else {
            return String{ entry_->d_name };
        }
    }

    bool is_directory() const {
        struct stat* st = get_stat();
        return S_ISDIR(stat_->st_mode);
    }

    bool is_end() const { return entry_ == nullptr; }

private:
    struct dirent* entry_ = nullptr;
    mutable std::shared_ptr<struct stat> stat_;

    PosixPath from_dir_;
};

class FileIterImplUnix {
private:
    void next() {
        do {
            auto dir_entry = ::readdir(dir_);
            dir_entry_ = FileInfoImplUnix{ dir_entry, dir_path_ };
        } while (dir_entry_.name() == "." || dir_entry_.name() == "..");
    }

public:
    FileIterImplUnix() {}

    ~FileIterImplUnix() {
        if (dir_) {
            closedir(dir_);
        }
    }

    FileIterImplUnix(const char* path)
        : dir_path_(path) {
        dir_ = ::opendir(path);
        next();
    }

    FileIterImplUnix(const String& path)
        : FileIterImplUnix(path.c_str()) {}

    FileIterImplUnix(const PosixPath& path)
        : FileIterImplUnix(path.c_str()) {}

    bool is_end() const {
        if (!dir_) {
            throw RuntimeError("Called is_end() for non-initialized iterator");
        }
        return dir_entry_.is_end();
    }

    bool is_directory() const { return dir_entry_.is_directory(); }

    const PosixPath& dir_path() const { return dir_path_; }

    PosixPath path() const {
        if (!dir_) {
            throw RuntimeError("Called path() for non-initialized iterator");
        }
        return PosixPath{ dir_path_, dir_entry_.name() };
    }

    bool operator==(const FileIterImplUnix& other) const {
        return dir_entry_.is_end() && other.dir_entry_.is_end();
    }

    bool operator!=(const FileIterImplUnix& other) const { return !(*this == other); }

    FileIterImplUnix& operator++() {
        if (!dir_) {
            throw RuntimeError("Called next file for non-initialized iterator");
        }
        next();
        return *this;
    }

    const FileInfoImplUnix& operator*() const { return dir_entry_; }

private:
    PosixPath dir_path_;
    DIR* dir_ = nullptr;
    FileInfoImplUnix dir_entry_;
};

typedef FileInfoImplUnix FileInfo;
typedef FileIterImplUnix FileIter;

class PathImplUnix : public PosixPath {
public:
    using Self = PathImplUnix;

    PathImplUnix() = default;

    PathImplUnix(String path)
        : PosixPath{ path } {}

    PathImplUnix(PosixPath path)
        : PosixPath{ std::move(path) } {}

    PathImplUnix(const char* path)
        : PosixPath{ path } {}

    template <typename... Types>
    PathImplUnix(Types... args)
        : PosixPath{ args... } {}

    const char* path_to_host() const { return Self::path_to_host(*this); }

    static const char* path_to_host(const PathImplUnix& path) { return path.c_str(); }

    static PathImplUnix tmp_dir() {
        static const char* tmp_path = getenv("TMPDIR");
        static Self tmp;
        if (tmp_path) {
            tmp = tmp_path;
        } else {
            tmp = "/tmp";
            // HACK FOR UNIX? FIXME
            // throw RuntimeError("No TMPDIR in env");
        }

        return tmp;
    }

    static const PathImplUnix cwd() {
        char buf[2000]; // FIXME: Handle bigger size and error
        const auto res = ::getcwd(buf, sizeof(buf));
        // check_error(res);
        return Self{ res };
    }

    PathImplUnix abspath() const { return Self::abspath(*this); }

    static PathImplUnix abspath(const PathImplUnix& path) { return Self{ cwd(), path }; }

    const PathImplUnix& mkdir() const { return Self::mkdir(*this); }

    static const PathImplUnix& mkdir(const PathImplUnix& path) {
        const auto res = ::mkdir(path_to_host(path), 0777);
        check_error(res);
        return path;
    }

    static const PathImplUnix& mkdir_if_not_exists(const PathImplUnix& path) {
        if (!path.exists()) {
            path.mkdir();
        }
        return path;
    }

    const PathImplUnix& mkdir_if_not_exists() const { return Self::mkdir_if_not_exists(*this); }

    static const PathImplUnix& mkdir_parents(const PathImplUnix& path) {
        Self cur_path;
        for (const auto& dir : path.split()) {
            cur_path = join(cur_path, dir);
            cur_path.mkdir_if_not_exists();
        }
        return path;
    }

    const PathImplUnix& mkdir_parents() const { return Self::mkdir_parents(*this); }

    static const PathImplUnix& rm(const PathImplUnix& path) {
        const auto res = ::remove(path.path_to_host());
        check_error(res);
        return path;
    }

    const PathImplUnix& rm() const { return Self::rm(*this); }

    static const PathImplUnix& rmrf(const PathImplUnix& path) {
        FileIterImplUnix iter{ path.str() };
        while (!iter.is_end()) {
            if (iter.is_directory()) {
                Self{ iter.path() }.rmrf();
            } else {
                Self{ iter.path() }.rm();
            }
            ++iter;
        }
        path.rm();
        return path;
    }

    const PathImplUnix& rmrf() const { return Self::rmrf(*this); }

    static const PathImplUnix& rmrf_if_exists(const PathImplUnix& path) {
        if (path.exists()) {
            return path.rmrf();
        }
        return path;
    }

    const PathImplUnix& rmrf_if_exists() const { return Self::rmrf_if_exists(*this); }

    bool exists() const { return Self::exists(*this); }

    static bool exists(const PathImplUnix& path) {
        struct stat st;
        const auto res = ::stat(path.path_to_host(), &st);
        return res == 0;
    }

    operator String &&() { return std::move(str_move()); }
};

typedef PathImplUnix Path;

bool operator==(const Path& path_a, const char* path_b) { return path_a.str() == path_b; }

bool operator==(const Path& path_a, const String& path_b) { return path_a.str() == path_b; }

Path operator/(const Path& to, const char* add) { return Path{ to, add }; }

Path operator/(const Path& to, const String& add) { return Path{ to, add }; }

class IterPath {
public:
    typedef FileIter const_iterator;

    IterPath(Path path)
        : path_{ path } {}

    const String& str() const { return path_.str(); }

private:
    Path path_;
};

static const IterPath iter_dir(const Path& path) { return IterPath{ path }; }

IterPath::const_iterator begin(const IterPath& path) {
    return IterPath::const_iterator{ path.str() };
}

IterPath::const_iterator end(const IterPath& path) { return IterPath::const_iterator{}; }

bool is_abspath(const String& path) { return Path::is_abspath(path); }

Path cwd() { return Path::cwd(); }

Path cd(const Path& path) { throw NotImplementedException(); }

Path tmp_dir() { return Path::tmp_dir(); }

Path user_dir() { throw NotImplementedException(); }

template <typename... Types> String join(Types... args) { return Path::join(args...).str(); }

struct WatchEvent {
    enum Type {
        WatchDirectoryDestroyed, // the watched directory was destroyed
        FileCreated,
        FileDeleted,
        FileModified
    };

    Type type;
    std::string name;

    WatchEvent()
        : type(WatchDirectoryDestroyed)
        , name({}) {}

    WatchEvent(Type type_, const std::string& name_)
        : type(type_)
        , name(name_) {}
};

namespace {

    template <typename PoolType> struct generic_directory_watch {
        using id_type = typename PoolType::id_type;
        using pool_type = PoolType;

        std::string path;
        PoolType* pool;
        id_type nativeHandle = -1;
        int ticket = -1;
        bool dead = true;

        generic_directory_watch()
            : pool(0)
            , dead(true) {}

        explicit generic_directory_watch(const std::string& path_, PoolType* poolPtr)
            : path(path_)
            , pool(poolPtr) {
            recreate();
        }

        void destroy() {
            pool->destroy(nativeHandle);
            dead = true;
        }

        void recreate() {
            destroy();

            auto result = pool->create(path.c_str());
            if (result.error == 0) {
                dead = false;
                nativeHandle = result.handle;
                ticket = result.ticket;
            }
        }

        ~generic_directory_watch() { destroy(); }

        bool pollEvent(WatchEvent& event) {
            if (dead)
                recreate();
            if (dead) // bail
                return false;

            pool->update();
            auto vec = pool->getEvents(nativeHandle);
            if (static_cast<size_t>(ticket) >= vec.size())
                return false;

            event = vec.at(ticket++);

            if (event.type == WatchEvent::WatchDirectoryDestroyed)
                dead = true;

            return true;
        }
    };

    template <typename DirectoryWatcherType> struct generic_file_watcher {
        DirectoryWatcherType directoryWatcher;
        std::string filename;

        static std::string getDirectory(const std::string& dir) {
            for (auto iter = dir.rbegin(); iter != dir.rend(); iter++) {
                if (*iter == '/' || *iter == '\\')
                    return std::string(dir.begin(), dir.end() - std::distance(dir.rbegin(), iter));
            }
            return {};
        }

        static std::string getFilename(const std::string& dir) {
            for (auto iter = dir.rbegin(); iter != dir.rend(); iter++) {
                if (*iter == '/' || *iter == '\\')
                    return std::string(dir.end() - std::distance(dir.rbegin(), iter), dir.end());
            }
            return {};
        }

        generic_file_watcher()
            : directoryWatcher() {}

        explicit generic_file_watcher(
            const std::string& dir, typename DirectoryWatcherType::pool_type* ptr)
            : directoryWatcher(getDirectory(dir), ptr)
            , filename(getFilename(dir)) {}

        generic_file_watcher(const std::string& dir, const std::string& file)
            : directoryWatcher(dir)
            , filename(file) {}

        bool pollEvent(WatchEvent& event) {
            while (directoryWatcher.pollEvent(event)) {
                if (event.name == filename)
                    return true;
            }
            return false;
        }
    };

    class no_copy {
    public:
        no_copy() = default;
        no_copy(no_copy&) = delete;
        no_copy(no_copy&&) = delete;
        no_copy operator=(const no_copy&) = delete;
    };

    class inotify_watch_pool : public no_copy {
    public:
        using id_type = int;

    private:
        int handleInotify_;

        std::unordered_map<id_type, std::vector<WatchEvent>> events_ = {};

        unsigned char* eventBuffer_ = (unsigned char*)std::calloc(1, 4096);

        constexpr static uint32_t DeadFlags = (IN_IGNORED | IN_Q_OVERFLOW | IN_UNMOUNT);
        constexpr static uint32_t FileCreatedFlags = (IN_CREATE | IN_MOVED_TO);
        constexpr static uint32_t FileDeletedFlags = (IN_MOVED_FROM | IN_DELETE);
        constexpr static uint32_t FileModifiedFlags = (IN_MODIFY);

        uint32_t translateToFlags(WatchEvent::Type event) {
            using ev = WatchEvent::Type;
            if (event == ev::FileCreated)
                return FileCreatedFlags;
            if (event == ev::FileDeleted)
                return FileDeletedFlags;
            if (event == ev::FileModified)
                return FileModifiedFlags;
            return 0;
        }

        void parseEvent(inotify_event& event) {
            std::vector<WatchEvent>& vec = events_[event.wd];

            if ((event.mask & DeadFlags) != 0) {
                // dead
                vec.emplace_back();
            } else {
                if ((event.mask & FileCreatedFlags) != 0)
                    vec.emplace_back(WatchEvent::FileCreated, std::string(event.name));

                else if ((event.mask & FileDeletedFlags) != 0)
                    vec.emplace_back(WatchEvent::FileDeleted, std::string(event.name));

                else if ((event.mask & FileModifiedFlags) != 0)
                    vec.emplace_back(WatchEvent::FileModified, std::string(event.name));
            }
        }

    public:
        inotify_watch_pool()
            : handleInotify_(inotify_init1(IN_NONBLOCK)) {}

        ~inotify_watch_pool() { std::free(eventBuffer_); }

        struct create_result {
            int error = 0;
            id_type handle;
            std::vector<WatchEvent>::size_type ticket;
        };

        create_result create(const char* file) {
            uint32_t flags = FileCreatedFlags | FileDeletedFlags | FileModifiedFlags;

            id_type handle = inotify_add_watch(handleInotify_, file, flags);

            create_result result;
            result.error = (handle == -1 ? errno : 0);
            result.handle = handle;
            result.ticket = (handle == -1 ? 0 : events_[handle].size());
            return result;
        }

        void destroy(id_type id) {
            if (id == -1)
                return;

            // TODO : invalid read on watch dtor here (valgrind)
            inotify_rm_watch(handleInotify_, id);
        }

        void update() {
            constexpr static uint32_t MaxEventSize = sizeof(inotify_event) + NAME_MAX + 1;
            ssize_t len = read(handleInotify_, eventBuffer_, MaxEventSize);
            ssize_t offset = 0;

            if (len == -1)
                return;

            while (len > offset) {
                inotify_event* ev = (inotify_event*)(offset + eventBuffer_);
                parseEvent(*ev);
                offset += sizeof(ev->mask) + sizeof(ev->wd) + sizeof(ev->cookie) + sizeof(ev->len)
                    + ev->len;
            }
        }

        const std::vector<WatchEvent>& getEvents(id_type watch) { return events_[watch]; }
    };
}

using global_watch_pool_type = inotify_watch_pool;
using DirectoryWatcher = generic_directory_watch<global_watch_pool_type>;
using FileWatcher = generic_file_watcher<DirectoryWatcher>;

}
