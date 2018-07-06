﻿// Gui
#include <luiconf.h>
#include <core/ui_window.h>
#include <util/ui_unicode.h>
#include <core/ui_manager.h>
#include <core/ui_string.h>
#include <input/ui_kminput.h>
#include <style/ui_ssvalue.h>
#include <filesystem/ui_file.h>
#include <core/ui_color_list.h>
#include <container/pod_hash.h>
#include <graphics/ui_cursor.h>
#include <control/ui_viewport.h>
#include <util/ui_color_system.h>
#include <graphics/ui_graphics_decl.h>
// C++
#include <cassert>
#include <algorithm>

// Windows
#define NOMINMAX
#include <Windows.h>
#include <Dwmapi.h>

#ifndef NDEBUG
#include <util/ui_time_meter.h>
#include "../private/ui_private_control.h"
#endif

// error beep
extern "C" void longui_error_beep();

// LongUI::detail
namespace LongUI { namespace detail {
    // lowest common ancestor
    auto lowest_common_ancestor(UIControl* now, UIControl* old) noexcept {
        // 由于控件有深度信息, 所以可以进行优化
        // 时间复杂度 O(q) q是目标解与最深条件节点之间深度差


        // now不能为空
        assert(now && "new one cannot be null");
        // old为空则返回now
        if (!old) return now;
        // 连接到相同深度
        UIControl* upper, *lower;
        if (now->GetLevel() < old->GetLevel()) {
            // 越大就是越低
            lower = old; upper = now;
        }
        else {
            // 越小就是越高
            lower = now; upper = old;
        }
        // 将低节点调整至于高节点同一水平
        auto adj = lower->GetLevel() - upper->GetLevel();
        while (adj) {
            lower = lower->GetParent();
            --adj;
        }
        // 共同遍历
        while (upper != lower) {
            assert(upper->IsTopLevel() == false);
            assert(lower->IsTopLevel() == false);
            upper = upper->GetParent();
            lower = lower->GetParent();
        }
        assert(upper && lower && "cannot be null");
        return upper;
    }
}}

/// <summary>
/// Sets the control world changed.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetControlWorldChanged(UIControl& ctrl) noexcept {
    assert(UIControlPrivate::TestWorldChanged(ctrl));
    assert(ctrl.GetWindow() == this);
    m_pTopestWcc = detail::lowest_common_ancestor(&ctrl, m_pTopestWcc);
    //LUIDebug(Log) << ctrl << ctrl.GetLevel() << endl;
    //if (m_pTopestWcc && m_pTopestWcc->GetLevel() < ctrl.GetLevel()) return;
    //m_pTopestWcc = &ctrl;
}

/// <summary>
/// Deletes the later.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::DeleteLater() noexcept {
    const auto view = &this->RefViewport();
    view->DeleteLater();
}

/// <summary>
/// Deletes this instance.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Delete() noexcept {
    const auto view = &this->RefViewport();
    delete view;
}

/// <summary>
/// Closes the window.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::close_window() noexcept {
    UIManager.close_helper(*this);
    this->RefViewport().DoEvent(nullptr, { NoticeEvent::Event_WindowClosed, 0 });
}

// ui namespace
namespace LongUI {
    // make style sheet
    auto MakeStyleSheet(U8View view, SSPtr old) noexcept->SSPtr;
    // make style sheet
    auto MakeStyleSheetFromFile(U8View view, SSPtr old) noexcept->SSPtr;
    // private msg
    struct PrivateMsg;
    /// <summary>
    /// Gets the size of the client.
    /// </summary>
    /// <param name="hwnd">The HWND.</param>
    /// <returns></returns>
    auto GetClientSize(HWND hwnd) noexcept -> Size2U {
        RECT client_rect; ::GetClientRect(hwnd, &client_rect);
        return {
            uint32_t(client_rect.right - client_rect.left),
            uint32_t(client_rect.bottom - client_rect.top)
        };
    }
    /// <summary>
    /// private data for CUIWindow
    /// </summary>
    class CUIWindow::Private : public CUIObject {
        // control map
        using CtrlMap = POD::HashMap<UIControl*>;
        // window list
        using Windows = POD::Vector<CUIWindow*>;
        // release data
        void release_data() noexcept;
        // begin render
        void begin_render() const noexcept;
        // begin render
        auto end_render() const noexcept->Result;
        // clean up
        //void cleanup() noexcept;
    public:
        // delete window
        static void DeleteWindow(HWND hwnd) noexcept;
        // reelase device data
        void ReleaseDeviceData() noexcept { this->release_data(); }
        // ctor
        Private() noexcept;
        // dtor
        ~Private() noexcept { this->release_data(); }
        // init
        HWND Init(HWND parent, CUIWindow::WindowConfig config) noexcept;
        // recreate_device
        auto Recreate(HWND hwnd) noexcept->Result;
        // render
        auto Render(const UIViewport& v) const noexcept->Result;
        // mark dirt rect
        void MarkDirtRect(const RectF & rect) noexcept;
        // before render
        void BeforeRender() noexcept;
        // close popup
        void ClosePopup() noexcept;
    public:
        // mouse event[thread safe]
        void DoMouseEventTs(const MouseEventArg& args) noexcept;
        // do msg
        auto DoMsg(const PrivateMsg&) noexcept ->intptr_t;
        // when create
        void OnCreate(HWND) noexcept {}
        // when resize[thread safe]
        void OnResizeTs(Size2U) noexcept;
        // when key down
        void OnKeyDown(CUIInputKM::KB key) noexcept;
        // when system key down
        void OnSystemKeyDown(CUIInputKM::KB key, uintptr_t lp) noexcept;
        // when input a utf-16 char
        void OnChar(char16_t ch) noexcept;
        // when input a utf-32 char[thread safe]
        void OnCharTs(char32_t ch) noexcept;
        // do hotkey
        void OnHotKey(uintptr_t id) noexcept;
    public:
        // full render this frame?
        inline bool is_full_render_for_update() const noexcept { return full_render_for_update; }
        // mark full rendering
        inline void mark_full_rendering_for_update() noexcept { full_render_for_update = true; }
    private: // TODO: is_xxxx
        // full render this frame?
        inline bool is_full_render_for_render() const noexcept { return full_render_for_render; }
        // mark full rendering
        inline void mark_full_rendering_for_render() noexcept { full_render_for_render = true; }
        // clear full rendering
        inline void clear_full_rendering_for_update() noexcept { full_render_for_update = false; }
        // clear full rendering
        inline void clear_full_rendering_for_render() noexcept { full_render_for_render = false; }
        // using direct composition?
        inline bool is_direct_composition() const noexcept { return false; }
        // is skip render?
        inline bool is_skip_render() const noexcept { return system_skip_rendering; }
    public:
        // register the window class
        static void RegisterWindowClass() noexcept;
    public:
        // swap chian
        I::Swapchan*    swapchan = nullptr;
        // bitmap buffer
        I::Bitmap*      bitmap = nullptr;
        // window clear color
        ColorF          clear_color = ColorF::FromRGBA_CT<RGBA_TianyiBlue>();
    public:
        // mouse track data
        TRACKMOUSEEVENT track_mouse;
    public:
        // popup window
        CUIWindow*      popup = nullptr;
        // viewport
        UIViewport*     viewport = nullptr;
        // now cursor
        CUICursor       cursor = { CUICursor::Cursor_Arrow };
        // rect of window
        RectWHL         rect = {};
        // window adjust
        RectL           adjust = {};
#ifndef NDEBUG
        // full render counter
        uint32_t        dbg_full_render_counter = 0;
        // dirty render counter
        uint32_t        dbg_dirty_render_counter = 0;
#endif
    public:
        // named control map
        CtrlMap         ctrl_map;
        // child windows
        Windows         children;
        // focused control
        UIControl*      focused = nullptr;
        // TODO: focused control - saved 
        //UIControl*      saved_focused = nullptr;
        // captured control
        UIControl*      captured = nullptr;
        // now default control
        UIControl*      now_default = nullptr;
        // window default control
        UIControl*      wnd_default = nullptr;
        // get first
        auto GetFirst() noexcept { return &this->focused; }
        // get last
        auto GetLast() noexcept { return &this->wnd_default; }
    public:
        // save utf-16 char
        char16_t        saved_utf16 = 0;
        // ma return code
        uint8_t         ma_return_code = 3;
        // sized
        bool            flag_sized : 1;
        // mouse enter
        bool            mouse_enter : 1;
        // accessibility called
        bool            accessibility : 1;
        // moving or resizing
        bool            moving_resizing : 1;
        // mouse left down
        bool            mouse_left_down : 1;
        // skip window via system
        bool            system_skip_rendering : 1;
    public:
        //// restore # update
        //bool            restore_for_update = false;
        //// restore # render
        //bool            restore_for_render = false;
        // full render for update
        bool            full_render_for_update = true;
        // full render for render
        bool            full_render_for_render = true;
        // access key display
        bool            access_key_display = false;
    public:
        // dirty count#1
        uint32_t        dirty_count_for_update = 0;
        // dirty count#2
        uint32_t        dirty_count_for_render = 0;
        // dirty rect#1
        RectF           dirty_rect_for_update[LongUI::DIRTY_RECT_COUNT];
        // dirty rect#2
        RectF           dirty_rect_for_render[LongUI::DIRTY_RECT_COUNT];
    public:
        // access key map
        UIControl*      access_key_map['Z' - 'A' + 1];
    private:
        // toggle access key display
        void toggle_access_key_display() noexcept;
        // resize window buffer
        void resize_window_buffer() noexcept;
        // forece resize
        void force_resize_window_buffer() const noexcept {
            const_cast<Private*>(this)->resize_window_buffer(); }
    };
    /// <summary>
    /// Privates the window.
    /// </summary>
    /// <returns></returns>
    LongUI::CUIWindow::Private::Private() noexcept {
        flag_sized = false;
        mouse_enter = false;
        accessibility = false;
        moving_resizing = false;
        mouse_left_down = false;
        system_skip_rendering = false;
        std::memset(access_key_map, 0, sizeof(access_key_map));
    }
}

/// <summary>
/// Initializes a new instance of the <see cref="CUIWindow" /> class.
/// </summary>
/// <param name="cfg">The config.</param>
/// <param name="parent">The parent.</param>
LongUI::CUIWindow::CUIWindow(CUIWindow* parent, WindowConfig cfg) noexcept :
m_parent(parent), config(cfg),
m_private(new(std::nothrow) Private) {
    // XXX: 错误处理
    if (!m_private) {
        m_bCtorFaild = true;
        return;
    }
    // 初始化
    m_private->viewport = &this->RefViewport();
    // 添加窗口
    if (parent) parent->add_child(*this);
    m_hwnd = m_private->Init(parent ? parent->GetHwnd() : nullptr, config);

    UIManager.add_window(*this);
}

/// <summary>
/// Makes the style sheet from file.
/// </summary>
/// <param name="file">The file.</param>
/// <param name="old">The old.</param>
/// <returns></returns>
auto LongUI::MakeStyleSheetFromFile(U8View file, SSPtr old) noexcept -> SSPtr {
    const CUIStringU8 old_dir = UIManager.GetXULDir();
    auto path = old_dir; path += file;
    // OOM处理?
    //if (!(old_dir.is_ok() && path.is_ok())) return old;
    // 获取CSS目录以便获取正确的文件路径
    auto view = path.view();
    while (view.end() > view.begin()) {
        --view.second; const auto ch = *view.end();
        if (ch == '/' || ch == '\\') break;
    }
    // 设置CSS目录作为当前目录
    UIManager.SetXULDir(view);
    // 待使用缓存
    POD::Vector<char> css_buffer;
    // 载入文件
    if (CUIFile css_file{ path.c_str(), CUIFile::Flag_Read }) {
        const auto file_size = css_file.GetFilezize();
        css_buffer.resize(file_size + 1);
        // OK
        if (css_buffer.is_ok()) {
            const auto ptr = &css_buffer.front();
            css_file.Read(ptr, file_size);
            ptr[file_size] = 0;
        }
    }
    // 字符缓存有效
    if (const auto len = css_buffer.size()) {
        const auto ptr = &css_buffer.front();
        U8View string{ ptr, ptr + len - 1 };
        old = LongUI::MakeStyleSheet(string, old);
    }
    // 设置之前的目录作为当前目录
    UIManager.SetXULDir(old_dir.view());
    return old;
}

/// <summary>
/// Loads the CSS file.
/// </summary>
/// <param name="file">The file.</param>
/// <returns></returns>
void LongUI::CUIWindow::LoadCSSFile(U8View file) noexcept {
    m_pStyleSheet = LongUI::MakeStyleSheetFromFile(file, m_pStyleSheet);
}

/// <summary>
/// Loads the CSS string.
/// </summary>
/// <param name="string">The string.</param>
/// <returns></returns>
void LongUI::CUIWindow::LoadCSSString(U8View string) noexcept {
    m_pStyleSheet = LongUI::MakeStyleSheet(string, m_pStyleSheet);
}


/// <summary>
/// Adds the child.
/// </summary>
/// <param name="child">The child.</param>
/// <returns></returns>
void LongUI::CUIWindow::add_child(CUIWindow& child) noexcept {
    // XXX: OOM 处理
    assert(m_private);
    m_private->children.push_back(&child);
}

/// <summary>
/// Removes the child.
/// </summary>
/// <param name="child">The child.</param>
/// <returns></returns>
void LongUI::CUIWindow::remove_child(CUIWindow& child) noexcept {
    // XXX: OOM 处理
    auto& vector = m_private->children;
    LongUI::RemovePointerItem(reinterpret_cast<PointerVector&>(vector), &child);
}

/// <summary>
/// Finalizes an instance of the <see cref="CUIWindow"/> class.
/// </summary>
/// <returns></returns>
LongUI::CUIWindow::~CUIWindow() noexcept {
#ifdef LUI_ACCESSIBLE
    if (m_pAccessible) {
        LongUI::FinalizeAccessible(*m_pAccessible);
        m_pAccessible = nullptr;
    }
#endif
    // 析构中
    m_inDtor = true;
    // 存在父窗口
    if (m_parent) {
        m_parent->remove_child(*this);
    }
    // 释放样式表
    LongUI::DeleteStyleSheet(m_pStyleSheet);
    // 有效私有数据
    if (m_private) {
        // 删除自窗口
        auto& children = m_private->children;
        while (!children.empty()) children.back()->Delete();
        // 管理器层移除引用
        UIManager.remove_window(*this);
        // 摧毁窗口
        Private::DeleteWindow(m_hwnd);
        // 删除数据
        delete m_private;
    }
}

/// <summary>
/// Pres the render.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::BeforeRender() noexcept {
    CUIDataAutoLocker locker;
    m_private->BeforeRender();
}

/// <summary>
/// Renders this instance.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::Render() const noexcept -> Result {
    assert(m_private && "bug: you shall not pass");
    // 可见可渲染
    if (this->IsVisible()) {
        return m_private->Render(this->RefViewport());
    }
    return{ Result::RS_OK };
}

namespace LongUI {
    /// <summary>
    /// Recursives the recreate_device.
    /// </summary>
    /// <param name="ctrl">The control.</param>
    /// <param name="release">if set to <c>true</c> [release].</param>
    /// <returns></returns>
    auto RecursiveRecreate(UIControl& ctrl, bool release) noexcept ->Result {
        Result hr = ctrl.Recreate(release);
        for (auto& x : ctrl) {
#if 0
            const auto code = LongUI::RecursiveRecreate(x, release);
            if (!code) hr = code;
#else
            hr = LongUI::RecursiveRecreate(x, release);
            if (!hr) break;
#endif
        }
        return hr;
    }
}

/// <summary>
/// Recreates the device data.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::RecreateDeviceData() noexcept -> Result {
    return LongUI::RecursiveRecreate(this->RefViewport(), false);
}

/// <summary>
/// Releases the device data.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::ReleaseDeviceData() noexcept {
    LongUI::RecursiveRecreate(this->RefViewport(), true);
}

/// <summary>
/// Releases the window only device.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::release_window_only_device() noexcept {
    m_private->ReleaseDeviceData();
}

/// <summary>
/// Recreates the window.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::recreate_window() noexcept -> Result {
    assert(m_private && "bug: you shall not pass");
    return m_private->Recreate(m_hwnd);
}

/// Gets the position.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::GetPos() const noexcept -> Point2L {
    return{ m_private->rect.left , m_private->rect.top };
}

/// <summary>
/// Registers the access key.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::RegisterAccessKey(UIControl& ctrl) noexcept {
    const auto ch = ctrl.GetAccessKey();
    if (ch >= 'A' && ch <= 'Z') {
        const auto index = ch - 'A';
        const auto map = m_private->access_key_map;
#ifndef NDEBUG
        if (map[index]) {
            LUIDebug(Warning)
                << "Access key("
                << ch
                << ") existed"
                << endl;
        }
#endif
        map[index] = &ctrl;
    }
    else assert(ch == 0 && "unsupported access key");
}

/// <summary>
/// Finds the control.
/// </summary>
/// <param name="id">The identifier.</param>
/// <returns></returns>
auto LongUI::CUIWindow::FindControl(const char* id) noexcept -> UIControl* {
    assert(id && "bad id");
    auto& map = m_private->ctrl_map;
    const auto itr = map.find(id);
    return itr != map.end() ? itr->second : nullptr;
}

/// <summary>
/// Controls the attached.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::ControlAttached(UIControl& ctrl) noexcept {
    assert(!"TODO");
    //this->AddNamedControl(ctrl);
    //this->RegisterAccessKey(ctrl);
}

/// <summary>
/// Adds the named control.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::AddNamedControl(UIControl& ctrl) noexcept {
    // 注册命名控件
    if (this && *ctrl.GetID()) {
        // 必须没有被注册过
#ifndef NDEBUG
        if (this->FindControl(ctrl.GetID())) {
            LUIDebug(Error) LUI_FRAMEID
                << "add named control but id exist: "
                << ctrl.GetID()
                << endl;
            assert(!"id exist!");
        }
#endif
        // XXX: 错误处理
        m_private->ctrl_map.insert({ ctrl.GetID(), &ctrl });
    }
}

/// <summary>
/// Controls the disattached.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <remarks>
/// null this ptr acceptable
/// </remarks>
/// <returns></returns>
void LongUI::CUIWindow::ControlDisattached(UIControl& ctrl) noexcept {
    // 没有承载窗口就算了
    if (!this) return;
    // 析构中
    if (m_inDtor) return;
    // XXX: ControlDisattached

    // XXX: 暴力移除一般引用..?
    {
        const auto b = m_private->GetFirst();
        const auto e = m_private->GetLast() + 1;
        for (auto itr = b; itr != e; ++itr) {
            if (&ctrl == *itr) *itr = nullptr;
        }
    }
    // 移除访问键引用
    const auto ch = ctrl.GetAccessKey();
    if (ch >= 'A' && ch <= 'Z') {
        const auto index = ch - 'A';
        const auto map = m_private->access_key_map;
        // 移除引用
        if (map[index] == &ctrl) map[index] = nullptr;
        // 提出警告
#ifndef NDEBUG
        else {
            LUIDebug(Warning)
                << "map[index] != &ctrl:(map[index]:"
                << map[index]
                << ")"
                << endl;
        }
#endif

    }
    // 移除映射表中的引用
    if (*ctrl.GetID()) {
        // TODO: 实现删除接口
        auto& map = m_private->ctrl_map;
        const auto itr = map.find(ctrl.GetID());
        assert(itr != map.end());
        itr->second = nullptr;
    }
}

/// <summary>
/// Sets the focus.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
bool LongUI::CUIWindow::SetFocus(UIControl& ctrl) noexcept {
    // 不可聚焦
    if (!ctrl.IsFocusable()) return false;
    // 焦点控件
    auto& focused = m_private->focused;
    // 当前焦点不能是待聚焦控件的祖先控件
#ifndef DEBUG
    if (focused) {
        assert(
            !ctrl.IsAncestorForThis(*focused) && 
            "cannot set focus to control that ancestor was focused"
        );
    }
#endif
    // 已为焦点
    if (focused == &ctrl) return true;
    // 释放之前焦点
    if (focused) this->KillFocus(*focused);
    // 设为焦点
    focused = &ctrl;
    ctrl.StartAnimation({ StyleStateType::Type_Focus, true });
    return true;
}


/// <summary>
/// Sets the capture.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetCapture(UIControl& ctrl) noexcept {
    m_private->captured = &ctrl;
    //LUIDebug(Hint) << ctrl << endl;
}

/// <summary>
/// Releases the capture.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
bool LongUI::CUIWindow::ReleaseCapture(UIControl& ctrl) noexcept {
    if (&ctrl == m_private->captured) {
        //LUIDebug(Hint) << ctrl << endl;
        assert(m_private->captured);
        m_private->captured = nullptr;
        return true;
    }
    return false;
}

/// <summary>
/// Kills the focus.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::KillFocus(UIControl& ctrl) noexcept {
    if (m_private->focused == &ctrl) {
        m_private->focused = nullptr;
        //m_private->saved_focused = nullptr;
        ctrl.StartAnimation({ StyleStateType::Type_Focus, false });
    }
}

/// <summary>
/// Resets the default.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::ResetDefault() noexcept {
    if (m_private->wnd_default) {
        this->SetDefault(*m_private->wnd_default);
    }
}

/// <summary>
/// Marks the dirt rect.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
void LongUI::CUIWindow::MarkDirtRect(const RectF& rect) noexcept {
    assert(m_private && "bug: you shall not pass");
    assert(LongUI::GetArea(rect) > 0.f);
    m_private->MarkDirtRect(rect);
}


/// <summary>
/// Marks the full rendering.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::MarkFullRendering() noexcept {
    m_private->mark_full_rendering_for_update();
}

/// <summary>
/// Determines whether [is full render this frame].
/// </summary>
/// <returns></returns>
bool LongUI::CUIWindow::IsFullRenderThisFrame() const noexcept {
    assert(m_private && "bug: you shall not pass");
    return m_private->is_full_render_for_update();
}

/// <summary>
/// Sets the default.
/// </summary>
/// <param name="ctrl">The control.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetDefault(UIControl& ctrl) noexcept {
    assert(this && "null this ptr");
    if (!ctrl.IsDefaultable()) return;
    constexpr auto dtype = StyleStateType::Type_Default;
    auto& nowc = m_private->now_default;
    if (nowc) nowc->StartAnimation({ dtype, false });
    (nowc = &ctrl)->StartAnimation({ dtype, true });
}

/// <summary>
/// Sets the color of the clear.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
void LongUI::CUIWindow::SetClearColor(const ColorF& color) noexcept {
    m_private->clear_color = color;
}

/// <summary>
/// Sets the now cursor.
/// </summary>
/// <param name="cursor">The cursor.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetNowCursor(const CUICursor& cursor) noexcept {
    m_private->cursor = cursor;
}

/// <summary>
/// Sets the now cursor.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
void LongUI::CUIWindow::SetNowCursor(std::nullptr_t) noexcept {
    m_private->cursor = { CUICursor::Cursor_Arrow };
}

/// <summary>
/// Called when [resize].
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnResizeTs(Size2U size) noexcept {
    assert(size.width && size.height && "bad size");
    // 不一样才处理
    const auto samew = this->rect.width == size.width;
    const auto sameh = this->rect.height == size.height;
    if (samew && sameh) return;
    this->mark_full_rendering_for_update();
    // 数据锁
    CUIDataAutoLocker locker;
    // 修改
    this->rect.width = size.width;
    this->rect.height = size.height;
    const auto fw = static_cast<float>(size.width);
    const auto fh = static_cast<float>(size.height);
    // 重置大小
    this->viewport->resize_window({ fw, fh });
    // 修改窗口缓冲帧大小
    this->flag_sized = true;
}

// ----------------------------------------------------------------------------
// ------------------- Windows
// ----------------------------------------------------------------------------



/// <summary>
/// Resizes the specified size.
/// </summary>
/// <param name="size">The size.</param>
/// <returns></returns>
void LongUI::CUIWindow::Resize(Size2L size) noexcept {
    const auto pimpl = m_private;
    // 不一样才处理
    const auto& old = pimpl->rect;
    if (old.width == size.width && old.height == size.height) {
        pimpl->mark_full_rendering_for_update();
        return;
    }
    // 内联窗口
    if (this->IsInlineWindow()) {
        assert(!"NOT IMPL");
    }
    else {
        // 调整大小
        const auto realw = size.width + pimpl->adjust.right - pimpl->adjust.left;
        const auto realh = size.height + pimpl->adjust.bottom - pimpl->adjust.top;
        // 改变窗口
        constexpr UINT flag = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS;
        ::SetWindowPos(m_hwnd, nullptr, 0, 0, realw, realh, flag);
    }
}


/// <summary>
/// Maps to screen.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
void LongUI::CUIWindow::MapToScreen(RectF& rect) const noexcept {
    // 内联窗口
    if (this->IsInlineWindow()) {
        assert(!"NOT IMPL");
    }
    // 系统窗口
    else {
        POINT pt{ 0, 0 };
        ::ClientToScreen(m_hwnd, &pt);
        const auto px = static_cast<float>(pt.x);
        const auto py = static_cast<float>(pt.y);
        rect.top += py;
        rect.left += px;
        rect.right += px;
        rect.bottom += py;
    }
}

/// <summary>
/// Maps to screen.
/// </summary>
/// <param name="pos">The position.</param>
/// <returns></returns>
void LongUI::CUIWindow::MapToScreen(Point2F& pos) const noexcept {
    RectF rect = { pos.x, pos.y, pos.x, pos.y };
    this->MapToScreen(rect);
    pos = { rect.left, rect.top };
}


/// <summary>
/// Maps from screen.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
void LongUI::CUIWindow::MapFromScreen(Point2F& pos) const noexcept {
    // 内联窗口
    if (this->IsInlineWindow()) {
        assert(!"NOT IMPL");
    }
    // 系统窗口
    else {
        POINT pt{ 0, 0 };
        ::ScreenToClient(m_hwnd, &pt);
        const auto px = static_cast<float>(pt.x);
        const auto py = static_cast<float>(pt.y);
        pos.x += px;
        pos.y += py;
    }
}

/// <summary>
/// Shows the window.
/// </summary>
/// <param name="sw">The sw type.</param>
/// <returns></returns>
void LongUI::CUIWindow::show_window(TypeShow sw) noexcept {
    assert(m_hwnd && "bad window");
    ::ShowWindow(m_hwnd, sw);
}

/// <summary>
/// Closes the window.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::CloseWindow() noexcept {
    ::PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
}

/// <summary>
/// Actives the window.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::ActiveWindow() noexcept {
    ::SetActiveWindow(m_hwnd);
}


/// <summary>
/// Determines whether this instance is visible.
/// </summary>
/// <returns></returns>
bool LongUI::CUIWindow::IsVisible() const noexcept {
    return !!::IsWindowVisible(m_hwnd);
}

/// <summary>
/// Sets the name of the title.
/// </summary>
/// <param name="name">The name.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetTitleName(const wchar_t* name) noexcept {
    assert(name && "bad name");
    assert(m_hwnd && "bad window");
    ::DefWindowProcW(m_hwnd, WM_SETTEXT, WPARAM(0), LPARAM(name));
}

/// <summary>
/// Popups the window.
/// </summary>
/// <param name="wnd">The WND.</param>
/// <param name="pos">The position.</param>
/// <returns></returns>
void LongUI::CUIWindow::PopupWindow(CUIWindow& wnd, Point2F pos) noexcept {
    auto& this_popup = m_private->popup;
    // 再次显示就是关闭
    if (this_popup == &wnd) {
        this->ClosePopup();
    }
    else {
        this->ClosePopup();
        this_popup = &wnd;
        // 计算位置
        const auto& view = wnd.RefViewport();
        this->MapToScreen(pos);
        const auto x = static_cast<int32_t>(pos.x);
        wnd.SetPos({ x, static_cast<int32_t>(pos.y) });
        wnd.ShowNoActivate();
    }
}

/// <summary>
/// Closes the popup.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::ClosePopup() noexcept {
    m_private->ClosePopup();
}

/// <summary>
/// Sets the position.
/// </summary>
/// <param name="pos">The position.</param>
/// <returns></returns>
void LongUI::CUIWindow::SetPos(Point2L pos) noexcept {
    assert(m_private && "bug: you shall not pass");
    auto& this_pos = reinterpret_cast<Point2L&>(m_private->rect.left);
    // 无需移动窗口
    if (this_pos.x == pos.x && this_pos.y == pos.y) return; this_pos = pos;
    // 内联窗口
    if (this->IsInlineWindow()) {
        assert(!"NOT IMPL");
    }
    // 系统窗口
    else {
        const auto adjx = 0; // m_private->adjust.left;
        const auto adjy = 0; // m_private->adjust.top;
        constexpr UINT flag = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
        ::SetWindowPos(m_hwnd, nullptr, pos.x + adjx, pos.y + adjy, 0, 0, flag);
    }
}

/// <summary>
/// Called when [key down].
/// </summary>
/// <param name="vk">The vk.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnKeyDown(CUIInputKM::KB key) noexcept {
    constexpr auto ekey = LongUI::InputEvent::Event_KeyDown;
    // 回车键?
    if (key == CUIInputKM::KB_RETURN) {
        // 直接将输入引导到默认控件
        if (const auto defc = this->now_default) {
            defc->DoInputEvent({ ekey, key });
            return;
        }
    }
    // 直接将输入引导到焦点控件
    if (const auto focused_ctrl = this->focused) {
        // 检查输出
        const auto rv = focused_ctrl->DoInputEvent({ ekey, key });
        // 回车键无视了?!
        if (rv == Event_Ignore && key == CUIInputKM::KB_RETURN) {
            // 直接将输入引导到默认控件
            if (const auto defc = this->now_default) {
                defc->DoInputEvent({ ekey, key });
                return;
            }
        }
    }
}

/// <summary>
/// Called when [system key down].
/// </summary>
/// <param name="key">The key.</param>
/// <param name="lp">The lp.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnSystemKeyDown(CUIInputKM::KB key, uintptr_t lp) noexcept {
    // 检查访问键
    if (key == CUIInputKM::KB_MENU) {
        // 只有按下瞬间有效, 后续的重复触发不算
        if (!(lp & (1 << 30))) 
            this->toggle_access_key_display();
    }
}


/// <summary>
/// Toggles the access key display.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::toggle_access_key_display() noexcept {
    auto& key = this->access_key_display;
    key = !key;
    const EventArg arg{ NoticeEvent::Event_ShowAccessKey, key };
    for (auto ctrl : this->access_key_map) {
        if (ctrl) ctrl->DoEvent(nullptr, arg);
    }
}

// c
extern "C" {
    // char16 -> char32
    char32_t impl_char16x2_to_char32(char16_t lead, char16_t trail);
}

/// <summary>
/// Called when [character].
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnChar(char16_t ch) noexcept {
    char32_t ch32;
    // UTF16 第一字
    if (Unicode::IsHighSurrogate(ch)) { this->saved_utf16 = ch; return; }
    // UTF16 第二字
    else if (Unicode::IsLowSurrogate(ch)) {
        ch32 = impl_char16x2_to_char32(this->saved_utf16, ch);
        this->saved_utf16 = 0;
    }
    // UTF16 仅一字
    else ch32 = static_cast<char32_t>(ch);
    // 处理输入
    this->OnCharTs(ch32);
}

/// <summary>
/// Called when [character].
/// </summary>
/// <param name="ch">The ch.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnCharTs(char32_t ch) noexcept {
    // TODO: 自己检查有效性?
    if (ch >= 0x20 || ch == '\t') {
        // 直接将输入引导到焦点控件
        if (const auto focused_ctrl = this->focused) {
            UIManager.DataLock();
            focused_ctrl->DoInputEvent({ LongUI::InputEvent::Event_Char, ch });
            UIManager.DataUnlock();
        }
    }
}

/// <summary>
/// Called when [hot key].
/// </summary>
/// <param name="id">The identifier.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::OnHotKey(uintptr_t id) noexcept {
    // A-Z
    if (id < ('Z' - 'A' + 1)) {
        UIManager.DataLock();
        const auto ctrl = this->access_key_map[id];
        if (ctrl) {
            ctrl->DoEvent(nullptr, { NoticeEvent::Event_DoAccessAction, 0 });
        }
        else ::longui_error_beep();
        UIManager.DataUnlock();
    }
}

// LongUI::detail
namespace LongUI { namespace detail {
    // window style
    enum style : DWORD {
        windowed         = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        aero_borderless  = WS_POPUP            | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
        basic_borderless = WS_POPUP            | WS_THICKFRAME              | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
    };
    // delete later
    enum msg : uint32_t {
        msg_custom = WM_USER + 10,
    };
}}

/// <summary>
/// Deletes the window.
/// </summary>
/// <param name="hwnd">The HWND.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::DeleteWindow(HWND hwnd) noexcept {
    ::DestroyWindow(hwnd);
}

/// <summary>
/// Initializes this instance.
/// </summary>
/// <param name="parent">The parent.</param>
/// <param name="config">The configuration.</param>
/// <returns></returns>
HWND LongUI::CUIWindow::Private::Init(HWND parent, CUIWindow::WindowConfig config) noexcept {
    // 尝试注册
    this->RegisterWindowClass();
    // 初始化
    HWND hwnd = nullptr;
    // 标题名称
    const wchar_t* titlename = nullptr;
    // 窗口
    {
        // 检查样式样式
        const DWORD window_style = config & CUIWindow::Config_Popup ?
            WS_POPUPWINDOW : WS_OVERLAPPEDWINDOW;
        this->ma_return_code = config & CUIWindow::Config_Popup ?
            MA_NOACTIVATE/*ANDEAT*/ : MA_ACTIVATE;
        // 调整大小
        static_assert(sizeof(RECT) == sizeof(this->adjust), "bad type");
        ::AdjustWindowRect(reinterpret_cast<RECT*>(&this->adjust), window_style, FALSE); 
        // 窗口
        RectWHL window_rect;
        window_rect.width = LongUI::DEFAULT_WINDOW_WIDTH;
        window_rect.height = LongUI::DEFAULT_WINDOW_HEIGHT;
        window_rect.width += this->adjust.right - this->adjust.left;
        window_rect.height += this->adjust.bottom - this->adjust.top;
        // 屏幕大小
        const auto scw = ::GetSystemMetrics(SM_CXFULLSCREEN);
        const auto sch = ::GetSystemMetrics(SM_CYFULLSCREEN);
        // 默认居中显示
        window_rect.left = (scw - window_rect.width) / 2;
        window_rect.top = (sch - window_rect.height) / 2;
        this->rect.left = window_rect.left;
        this->rect.top = window_rect.top;
        // 额外
        uint32_t ex_flag = 0;
        if (this->is_direct_composition()) ex_flag |= WS_EX_NOREDIRECTIONBITMAP;
        //if (config & CUIWindow::Config_Popup) ex_flag |= WS_EX_TOPMOST | WS_EX_NOACTIVATE;
        if (config & CUIWindow::Config_ToolWindow) ex_flag |= WS_EX_TOOLWINDOW;
        // 创建窗口
        hwnd = ::CreateWindowExW(
            //WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
            ex_flag,
            (config & CUIWindow::Config_Popup) ?
            Attribute::WindowClassNameP : Attribute::WindowClassNameN,
            titlename,
            window_style,
            window_rect.left, window_rect.top,
            window_rect.width, window_rect.height,
            parent,
            nullptr,
            ::GetModuleHandleA(nullptr),
            this
        );
        // TODO: 禁止 Alt + Enter 全屏
        //UIManager_DXGIFactory.MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);
    }
    // 创建成功
    if (hwnd) {
        // 注册所有的Alt+A-Z
        for (int i = 0; i != ('Z' - 'A' + 1); ++i) {
            ::RegisterHotKey(hwnd, i, MOD_ALT, 'A' + i);
        }

        //MARGINS shadow_state{ 1,1,1,1 };
        //::DwmExtendFrameIntoClientArea(hwnd, &shadow_state);
        //::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

        this->track_mouse.cbSize = sizeof(this->track_mouse);
        this->track_mouse.dwFlags = TME_HOVER | TME_LEAVE;
        this->track_mouse.hwndTrack = hwnd;
        this->track_mouse.dwHoverTime = HOVER_DEFAULT;
    }
    // 测试: 模式窗口
    if (parent) {
        //::EnableWindow(parent, false);
    }
    return hwnd;
}


/// <summary>
/// private msg
/// </summary>
struct LongUI::PrivateMsg { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; };


/// <summary>
/// Registers the window class.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::RegisterWindowClass() noexcept {
    const auto ins = ::GetModuleHandleW(nullptr);
    WNDCLASSEXW wcex;
    auto code = ::GetClassInfoExW(ins, Attribute::WindowClassNameN, &wcex);
    if (!code) {
        // 处理
        auto proc = [](HWND hwnd, 
            UINT message, 
            WPARAM wParam, 
            LPARAM lParam
            ) noexcept -> LRESULT {
            // 创建窗口时设置指针
            if (message == WM_CREATE) {
                // 获取指针
                auto* window = reinterpret_cast<CUIWindow::Private*>(
                    (reinterpret_cast<LPCREATESTRUCT>(lParam))->lpCreateParams
                    );
                // 设置窗口指针
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, LONG_PTR(window));
                // TODO: 创建完毕
                window->OnCreate(hwnd);
                // 返回1
                return 1;
            }
            // 其他情况则获取储存的指针
            else {
                const PrivateMsg msg { hwnd, message, wParam, lParam };
                const auto lptr = ::GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                const auto window = reinterpret_cast<CUIWindow::Private*>(lptr);
                if (!window) return ::DefWindowProcW(hwnd, message, wParam, lParam);
                return window->DoMsg(msg);
            }
        };
        // 注册一般窗口类
        wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = 0;
        wcex.lpfnWndProc = proc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(void*);
        wcex.hInstance = ins;
        wcex.hCursor = nullptr;
        wcex.hbrBackground = nullptr;
        wcex.lpszMenuName = nullptr;
        wcex.lpszClassName = Attribute::WindowClassNameN;
        wcex.hIcon = nullptr; // ::LoadIconW(ins, MAKEINTRESOURCEW(101));
        ::RegisterClassExW(&wcex);
        // 注册弹出窗口类
        wcex.style = CS_DROPSHADOW;
        wcex.lpszClassName = Attribute::WindowClassNameP;
        ::RegisterClassExW(&wcex);
    }
}

// ui namespace
namespace LongUI {
    // mouse event map(use char to avoid unnecessary memory-waste)
    const uint8_t MEMAP[] = {
        // WM_MOUSEMOVE                 0x0200
        static_cast<uint8_t>(MouseEvent::Event_MouseMove),
        // WM_LBUTTONDOWN               0x0201
        static_cast<uint8_t>(MouseEvent::Event_LButtonDown),
        // WM_LBUTTONUP                 0x0202
        static_cast<uint8_t>(MouseEvent::Event_LButtonUp),
        // WM_LBUTTONDBLCLK             0x0203
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_RBUTTONDOWN               0x0204
        static_cast<uint8_t>(MouseEvent::Event_RButtonDown),
        // WM_RBUTTONUP                 0x0205
        static_cast<uint8_t>(MouseEvent::Event_RButtonUp),
        // WM_RBUTTONDBLCLK             0x0206
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_MBUTTONDOWN               0x0207
        static_cast<uint8_t>(MouseEvent::Event_MButtonDown),
        // WM_MBUTTONUP                 0x0208
        static_cast<uint8_t>(MouseEvent::Event_MButtonUp),
        // WM_MBUTTONDBLCLK             0x0209
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_MOUSEWHEEL                0x020A
        static_cast<uint8_t>(MouseEvent::Event_MouseWheelV),
        // WM_XBUTTONDOWN               0x020B
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_XBUTTONUP                 0x020C
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_XBUTTONDBLCLK             0x020D
        static_cast<uint8_t>(MouseEvent::Event_Unknown),
        // WM_MOUSEHWHEEL               0x020E
        static_cast<uint8_t>(MouseEvent::Event_MouseWheelH),
    };
}

/// <summary>
/// Marks the dirt rect.
/// </summary>
/// <param name="rect">The rect.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::MarkDirtRect(const RectF& rect) noexcept {
    // 已经全渲染就算了
    if (!this->is_full_render_for_update()) {
        // 还在范围内
        if (this->dirty_count_for_update < LongUI::DIRTY_RECT_COUNT) {
            // 短名字而已
            const auto index = this->dirty_count_for_update;
            // 写入
            RectF write = rect;
            // 比较第一个
            if (index) {
                // 将面积最大的放在前面, 每次比较面积最大的
                const auto& first = this->dirty_rect_for_update[0];
                // 包含就算了
                if (LongUI::IsInclude(first, rect)) return;
                const auto s0 = LongUI::GetArea(first);
                const auto s1 = LongUI::GetArea(rect);
                // 不包含放在最后面
                if (s1 > s0) {
                    write = first;
                    this->dirty_rect_for_update[0] = rect;
                    // 反包含?
                    if (LongUI::IsInclude(rect, write)) return;
                }
            }
            // 写入数据
            this->dirty_rect_for_update[index] = write;
            ++this->dirty_count_for_update;
        }
        // 标记全渲染
        else {
            this->mark_full_rendering_for_update();
        }
    }
}

/// <summary>
/// Befores the render.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::BeforeRender() noexcept {
    // 先清除
    this->clear_full_rendering_for_render();
    // 复制全渲染信息
    if (this->is_full_render_for_update()) {
        this->mark_full_rendering_for_render();
        this->clear_full_rendering_for_update();
        this->dirty_count_for_render = 0;
        this->dirty_count_for_update = 0;
        return;
    }
    // 复制脏矩形信息
    this->dirty_count_for_render = this->dirty_count_for_update;
    this->dirty_count_for_update = 0;
    std::memcpy(
        this->dirty_rect_for_render,
        this->dirty_rect_for_update,
        sizeof(this->dirty_rect_for_update[0]) * this->dirty_count_for_render
    );
}

/// <summary>
/// Closes the popup.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::ClosePopup() noexcept {
    if (this->popup) {
        this->popup->CloseWindow();
        this->popup = nullptr;
    }
}


/// <summary>
/// Mouses the event.
/// </summary>
/// <param name="args">The arguments.</param>
/// <returns></returns>
void LongUI::CUIWindow::Private::DoMouseEventTs(const MouseEventArg & args) noexcept {
    CUIDataAutoLocker locker;
    UIControl* ctrl = this->captured;
    if (!ctrl) ctrl = this->viewport;
    ctrl->DoMouseEvent(args);
}

#ifdef LUI_ACCESSIBLE
#include <accessible/ui_accessible_win.h>

#endif

/// <summary>
/// Does the MSG.
/// </summary>
/// <param name="">The .</param>
/// <returns></returns>
auto LongUI::CUIWindow::Private::DoMsg(const PrivateMsg& prmsg) noexcept -> intptr_t {
    MouseEventArg arg;
    const auto hwnd = prmsg.hwnd;
    const auto message = prmsg.message;
    const auto lParam = prmsg.lParam;
    const auto wParam = prmsg.wParam;
    // 鼠标离开
    if (message == WM_MOUSELEAVE) {
        // BUG: Alt + 快捷键也会触发?
        //CUIInputKM::Instance().GetKeyState(CUIInputKM::KB_MENU);

        arg.px = -1.f; arg.py = -1.f;
        arg.wheel = 0.f;
        arg.type = MouseEvent::Event_MouseLeave;
        arg.modifier = LongUI::Modifier_None;
        this->mouse_enter = false;
        this->DoMouseEventTs(arg);
    }
    // 一般鼠标消息处理
    else if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) {
        // 检查映射表长度
        constexpr size_t ARYLEN = sizeof(MEMAP) / sizeof(MEMAP[0]);
        constexpr size_t MSGLEN = WM_MOUSELAST - WM_MOUSEFIRST + 1;
        static_assert(ARYLEN == MSGLEN, "must be same");
        // 初始化参数
        arg.type = static_cast<MouseEvent>(MEMAP[message - WM_MOUSEFIRST]);
        arg.wheel = 0.f;
        arg.px = static_cast<float>(int16_t(LOWORD(lParam)));
        arg.py = static_cast<float>(int16_t(HIWORD(lParam)));
        arg.modifier = static_cast<InputModifier>(wParam);
        // shift+滚轮自动映射到水平方向
        if (message == WM_MOUSEWHEEL && (wParam & MK_SHIFT)) {
            arg.type = MouseEvent::Event_MouseWheelH;
        }
        switch (arg.type)
        {
            float delta;
        case MouseEvent::Event_MouseWheelV:
        case MouseEvent::Event_MouseWheelH:
            // 滚轮消息则修改滚轮数据
            delta = static_cast<float>(int16_t(HIWORD(wParam)));
            arg.wheel = delta / static_cast<float>(WHEEL_DELTA);
            break;
        case MouseEvent::Event_MouseMove:
            // 第一次进入
            if (!this->mouse_enter) {
                arg.type = MouseEvent::Event_MouseEnter;
                this->mouse_enter = true;
                this->DoMouseEventTs(arg);
                arg.type = MouseEvent::Event_MouseMove;
            };
            // 鼠标跟踪
            ::TrackMouseEvent(&this->track_mouse);
            break;
        case MouseEvent::Event_LButtonDown:
            // 有弹出窗口先关闭
            if (this->popup) {
                this->popup->CloseWindow();
                this->popup = nullptr;
                return 0;
            }
            else {
                this->mouse_left_down = true;
                ::SetCapture(hwnd);
            }
            //LUIDebug(Hint) << "\r\n\t\tDown: " << this->captured << endl;
            break;
        case MouseEvent::Event_LButtonUp:
            // 没有按下就不算
            if (!this->mouse_left_down) return 0;
            this->mouse_left_down = false;
            //LUIDebug(Hint) << "\r\n\t\tUp  : " << this->captured << endl;
            ::ReleaseCapture();
            break;
        }
        this->DoMouseEventTs(arg);
    }
    // 其他消息处理
    else
        switch (message)
        {
        case WM_SHOWWINDOW:
            // TODO: popup?
            break;
        case WM_SETCURSOR:
            ::SetCursor(reinterpret_cast<HCURSOR>(this->cursor.GetHandle()));
            break;
#ifdef LUI_ACCESSIBLE
        case WM_DESTROY:
            if (this->accessibility) {
                ::UiaReturnRawElementProvider(hwnd, 0, 0, nullptr);
            }
            break;
        case WM_GETOBJECT:
            if (static_cast<long>(lParam) == static_cast<long>(UiaRootObjectId)) {
                const auto window = this->viewport->GetWindow();
                assert(window && "cannot be null");
                const auto root = CUIAccessibleWnd::FromWindow(*window);
                this->accessibility = true;
                return ::UiaReturnRawElementProvider(hwnd, wParam, lParam, root);
            }
            return 0;
#endif
        case WM_ENTERSIZEMOVE:
            this->moving_resizing = true;
            break;
        case WM_EXITSIZEMOVE:
            // TODO: flag检查是否允许拖动时缩放
            this->moving_resizing = false;
            this->OnResizeTs(LongUI::GetClientSize(hwnd));
            break;
        //case WM_NCCALCSIZE:
        //    if (wParam) return 0;
        //    break;
        case WM_SETFOCUS:
            return 0;
        case WM_KILLFOCUS:
            if (this->viewport->RefWindow().config & CUIWindow::Config_Popup) {
                // CloseWindow 仅仅最小化窗口
                ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        case WM_SIZE:
            // 最小化不算
            switch (wParam) { case SIZE_MINIMIZED: return 0; }
            // 拖拽重置不算
            if (this->moving_resizing) return 0;
            // 重置大小
            this->OnResizeTs({ LOWORD(lParam), HIWORD(lParam) });
            return 1;
        case WM_KEYDOWN:
            UIManager.DataLock();
            this->OnKeyDown(static_cast<CUIInputKM::KB>(wParam));
            UIManager.DataUnlock();
            return 1;
        case WM_SYSKEYDOWN:
            UIManager.DataLock();
            this->OnSystemKeyDown(static_cast<CUIInputKM::KB>(wParam), lParam);
            UIManager.DataUnlock();
            return 1;
        case WM_GETMINMAXINFO:
            [](const LPMINMAXINFO info) noexcept {
                // TODO: 窗口最小大小
                info->ptMinTrackSize.x += DEFAULT_CONTROL_WIDTH;
                info->ptMinTrackSize.y += DEFAULT_CONTROL_HEIGHT;
            }(reinterpret_cast<MINMAXINFO*>(lParam));
            return 0;
        case WM_CHAR:
            this->OnChar(static_cast<char16_t>(wParam));
            return 1;
        case WM_UNICHAR:
            this->OnCharTs(static_cast<char32_t>(wParam));
            return 1;
        case WM_MOVING:
            // LOCK: 加锁?
            this->rect.top = reinterpret_cast<RECT*>(lParam)->top;
            this->rect.left = reinterpret_cast<RECT*>(lParam)->left;
            return 1;
        case WM_CLOSE:
            this->viewport->RefWindow().close_window();
            return 0;
        case WM_MOUSEACTIVATE:
            return ma_return_code;
        case WM_NCACTIVATE:
            //LUIDebug(Log) << "WM_NCACTIVATE " << hwnd << LOWORD(wParam) << endl;
            //LUIDebug(Log) << hwnd << '(' << ::GetParent(hwnd)
            //    << ')' << LOWORD(wParam) << endl;
            // 非激活状态
            if (LOWORD(wParam) == WA_INACTIVE) {
                // 释放弹出窗口
                this->ClosePopup();
            }
            // 激活状态
            else {

            }
            break;
        case WM_HOTKEY:
            this->OnHotKey(wParam);
            break;
        }
    // 未处理消息
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
}


// ----------------------------------------------------------------------------
// ------------------- Graphics
// ----------------------------------------------------------------------------
#include <core/ui_manager.h>
#include <graphics/ui_graphics_impl.h>

#pragma comment(lib, "dxguid")
//#pragma comment(lib, "dwmapi")

/// <summary>
/// Recreates this instance.
/// </summary>
/// <param name="hwnd">The HWND.</param>
/// <returns></returns>
auto LongUI::CUIWindow::Private::Recreate(HWND hwnd) noexcept -> Result {
    if (this->is_skip_render()) return{ Result::RS_OK };
    CUIRenderAutoLocker locker;
    assert(this->bitmap == nullptr && "call release first");
    assert(this->swapchan == nullptr && "call release first");
    // 保证内存不泄漏
    this->release_data();
    Result hr = { Result::RS_OK };
    // 创建交换酱
    if (hr) {
        // 获取窗口大小
        RECT client_rect; ::GetClientRect(hwnd, &client_rect);
        const Size2U wndsize = {
            uint32_t(client_rect.right - client_rect.left),
            uint32_t(client_rect.bottom - client_rect.top)
        };
        // 交换链信息
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
        // TODO: DComp
        if (this->is_direct_composition()) {
            assert(!"NOTIMPL");
            swapChainDesc.Width = wndsize.width;
            swapChainDesc.Height = wndsize.height;
        }
        else {
            swapChainDesc.Width = wndsize.width;
            swapChainDesc.Height = wndsize.height;
        }
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        // TODO: 延迟等待
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        swapChainDesc.Flags = 0;
        // SWAP酱
        IDXGISwapChain1* sc = nullptr;
        // TODO: DComp
        if (this->is_direct_composition()) {
            
        }
        else {
            // 一般桌面应用程序
            swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            // 利用窗口句柄创建交换链
            hr = { UIManager.RefGraphicsFactory().CreateSwapChainForHwnd(
                &UIManager.Ref3DDevice(),
                hwnd,
                &swapChainDesc,
                nullptr,
                nullptr,
                &sc
            ) };
            longui_debug_hr(hr, L"GraphicsFactory.CreateSwapChainForHwnd faild");
        }
        // 设置 SWAP酱
        this->swapchan = static_cast<I::Swapchan*>(sc);
    }
    IDXGISurface* backbuffer = nullptr;
    // 利用交换链获取Dxgi表面
    if (hr) {
        hr = { this->swapchan->GetBuffer(
            0, IID_IDXGISurface, reinterpret_cast<void**>(&backbuffer)
        ) };
        longui_debug_hr(hr, L"swapchan->GetBuffer faild");
    }
    // 利用Dxgi表面创建位图
    if (hr) {
        D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        ID2D1Bitmap1* bmp = nullptr;
        hr = { UIManager.Ref2DRenderer().CreateBitmapFromDxgiSurface(
            backbuffer,
            &bitmapProperties,
            &bmp
        ) };
        this->bitmap = static_cast<I::Bitmap*>(bmp);
        longui_debug_hr(hr, L"2DRenderer.CreateBitmapFromDxgiSurface faild");
    }
    LongUI::SafeRelease(backbuffer);
    // TODO: 使用DComp
    if (this->is_direct_composition()) {

    }
    return hr;
}


/// <summary>
/// Releases the data.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::release_data() noexcept {
#ifndef NDEBUG
    //if (this->swapchan) {
    //    this->swapchan->AddRef();
    //    const auto count = this->swapchan->Release();
    //    int bk = 9;
    //}
#endif
    LongUI::SafeRelease(this->bitmap);
    LongUI::SafeRelease(this->swapchan);
}

/// <summary>
/// Resizes the window buffer.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::resize_window_buffer() noexcept {
    this->flag_sized = false;
    assert(this->rect.width && this->rect.height);
    // 检测是否无需修改
    const auto olds = this->bitmap->GetPixelSize();
    const auto samew = olds.width == this->rect.width;
    const auto sameh = olds.height == this->rect.height;
    if (samew && sameh) return;
    // 渲染区
    CUIRenderAutoLocker locker;
    // 取消引用
    UIManager.Ref2DRenderer().SetTarget(nullptr);
    // 强行重置flag
    bool force_resize = true;
    // DComp
    if (this->is_direct_composition()) {
        assert(!"NOT IMPL");
        force_resize = false;
    }
    // 重置缓冲帧大小
    if (force_resize) {
        IDXGISurface* dxgibuffer = nullptr;
        LongUI::SafeRelease(this->bitmap);
        // TODO: 延迟等待
        Result hr = { this->swapchan->ResizeBuffers(
            2, this->rect.width, this->rect.height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
            0
        ) };
        longui_debug_hr(hr, L"m_pSwapChain->ResizeBuffers faild");
        // TODO: RecreateResources 检查
        if (hr.code == DXGI_ERROR_DEVICE_REMOVED || 
            hr.code == DXGI_ERROR_DEVICE_RESET) {
            //UIManager.RecreateResources();
            LUIDebug(Hint) << L"Recreate device" << LongUI::endl;
        }
        // 利用交换链获取Dxgi表面
        if (hr) {
            hr = { this->swapchan->GetBuffer(
                0,
                IID_IDXGISurface,
                reinterpret_cast<void**>(&dxgibuffer)
            ) };
            longui_debug_hr(hr, L"m_pSwapChain->GetBuffer faild");
        }
        // 利用Dxgi表面创建位图
        if (hr) {
            D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            );
            ID2D1Bitmap1* bmp = nullptr;
            hr = { UIManager.Ref2DRenderer().CreateBitmapFromDxgiSurface(
                dxgibuffer,
                &bitmapProperties,
                &bmp
            ) };
            this->bitmap = static_cast<I::Bitmap*>(bmp);
            longui_debug_hr(hr, L"UIManager_RenderTarget.CreateBitmapFromDxgiSurface faild");
        }
        // 重建失败?
        LongUI::SafeRelease(dxgibuffer);
        LUIDebug(Hint)
            << "resize to"
            << this->rect.width
            << 'x'
            << this->rect.height
            << LongUI::endl;
        // TODO: 错误处理
        if (!hr) {
            LUIDebug(Error) << L" Recreate FAILED!" << LongUI::endl;
            UIManager.ShowError(hr);
        }
    }
}

/// <summary>
/// Renders this instance.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::Private::Render(const UIViewport& v) const noexcept->Result {
    // 跳过该帧?
    if (this->is_skip_render()) return { Result::RS_OK };
    // 请求渲染
    const auto count = this->dirty_count_for_render;
    if (this->is_full_render_for_render() || count) {
        // 重置窗口缓冲帧大小
        if (this->flag_sized) this->force_resize_window_buffer();
        // 开始渲染
        this->begin_render();
        // 矩形列表
        const auto rects = this->dirty_rect_for_render;
        // 正式渲染
        CUIControlControl::RecursiveRender(v, rects, count);
#if 0
        //if (this->viewport->GetWindow()->GetParent())
        LUIDebug(Log) << "End of Frame" << this << endl;
#endif
        // 结束渲染
        return this->end_render();
    }
    return{ Result::RS_OK };
}

/// <summary>
/// Begins the render.
/// </summary>
/// <returns></returns>
void LongUI::CUIWindow::Private::begin_render() const noexcept {
    // TODO: 完成
    auto& renderer = UIManager.Ref2DRenderer();
    // 设置文本渲染策略
    //renderer.SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    // 设为当前渲染对象
    renderer.SetTarget(this->bitmap);
    // 开始渲染
    renderer.BeginDraw();
    // 设置转换矩阵
#if 1
    renderer.SetTransform(D2D1::Matrix3x2F::Identity());
#else
    renderer.SetTransform(&impl::d2d(m_pViewport->box.world));
#endif
    // 清空背景
    renderer.Clear(auto_cast(clear_color));
}


/// <summary>
/// Ends the render.
/// </summary>
/// <returns></returns>
auto LongUI::CUIWindow::Private::end_render() const noexcept->Result {
    auto& renderer = UIManager.Ref2DRenderer();
    renderer.SetTransform({ 1.f,0.f,0.f,1.f,0.f,0.f });
    // TODO: 渲染插入符号

#ifndef NDEBUG
    // 显示
    if (UIManager.flag & IUIConfigure::Flag_DbgDrawDirtyRect) {
        // 水平扫描线: 全局渲染
        if (this->is_full_render_for_render()) {
            assert(this->rect.height);
            const float y = float(dbg_full_render_counter % this->rect.height);
            renderer.PushAxisAlignedClip(
                D2D1_RECT_F{ 0, y, float(rect.width), y + 1 },
                D2D1_ANTIALIAS_MODE_ALIASED
            );
            renderer.Clear(D2D1::ColorF{ 0x000000 });
            renderer.PopAxisAlignedClip();
        }
        // 随机颜色方格: 增量渲染
        else {
            static float s_color_value = 0.f;
            constexpr float s_color_step = 0.005f * 360.f;
            ColorSystem::HSLA hsl;
            hsl.a = 0.5f; hsl.s = 1.f; hsl.l = 0.36f; hsl.h = s_color_value;
            // 遍历脏矩形
            for (uint32_t i = 0; i != this->dirty_count_for_render; ++i) {
                const auto& src = this->dirty_rect_for_render[i];
                auto& bursh = CUIManager::RefCCBrush(hsl.toRGBA());
                renderer.FillRectangle(auto_cast(src), &bursh);
                hsl.h += s_color_step;
                if (hsl.h > 360.f) hsl.h = 0.f;
            }
            s_color_value = hsl.h;
        }
    }
#endif
    // 结束渲染
    Result hr = { renderer.EndDraw() };
    // 清除目标
    renderer.SetTarget(nullptr);
    // TODO: 完全渲染
    if (this->is_full_render_for_render()) {
#ifndef NDEBUG
        CUITimeMeterH meter;
        meter.Start();
        hr = { this->swapchan->Present(0, 0) };
        const auto time = meter.Delta_ms<float>();
        if (time > 9.f) {
            LUIDebug(Hint)
                << "present time: "
                << time
                << "ms"
                << LongUI::endl;
        }
        //assert(time < 1000.f && "took too much time, check plz.");
#else
        hr = { this->swapchan->Present(0, 0) };
#endif
        longui_debug_hr(hr, L"swapchan->Present full rendering faild");
    }
    // 增量渲染
    else {
        // 呈现参数设置
        RECT scroll = { 0, 0, this->rect.width, this->rect.height };
        RECT rects[LongUI::DIRTY_RECT_COUNT];
        // 转换为整型
        for (uint32_t i = 0; i != this->dirty_count_for_render; ++i) {
            const auto& src = this->dirty_rect_for_render[i];
            const auto right = static_cast<LONG>(std::ceil(src.right));
            const auto bottom = static_cast<LONG>(std::ceil(src.bottom));
            // 写入
            rects[i].top = std::max(static_cast<LONG>(src.top), 0l);
            rects[i].left = std::max(static_cast<LONG>(src.left), 0l);
            rects[i].right = std::min(right, scroll.right);
            rects[i].bottom = std::min(bottom, scroll.bottom);
        }
        // 填充参数
        DXGI_PRESENT_PARAMETERS present_parameters;
        present_parameters.DirtyRectsCount = this->dirty_count_for_render;
        present_parameters.pDirtyRects = rects;
        present_parameters.pScrollRect = &scroll;
        present_parameters.pScrollOffset = nullptr;
        // 提交该帧
#ifndef NDEBUG
        CUITimeMeterH meter;
        meter.Start();
        hr = { this->swapchan->Present1(0, 0, &present_parameters) };
        const auto time = meter.Delta_ms<float>();
        if (time > 9.f) {
            LUIDebug(Hint)
                << "present time: "
                << time
                << "ms"
                << LongUI::endl;
        }
        //assert(time < 1000.f && "took too much time, check plz.");
#else
        hr = { this->swapchan->Present1(0, 0, &present_parameters) };
#endif
        longui_debug_hr(hr, L"swapchan->Present full rendering faild");
    }
    // 收到重建消息/设备丢失时 重建UI
    constexpr int32_t DREMOVED = DXGI_ERROR_DEVICE_REMOVED;
    constexpr int32_t DRESET = DXGI_ERROR_DEVICE_RESET;
#ifdef NDEBUG
    if (hr.code == DREMOVED || hr.code == DRESET) {
        UIManager.NeedRecreate();
        hr = { Result::RS_FALSE };
    }
#else
    // TODO: 测试 test_D2DERR_RECREATE_TARGET
    if (hr.code == DREMOVED || hr.code == DRESET ) {
        UIManager.NeedRecreate();
        hr = { Result::RS_FALSE };
        LUIDebug(Hint) << "D2DERR_RECREATE_TARGET!" << LongUI::endl;
    }
    assert(hr);
    // 调试
    if (this->is_full_render_for_render())
        ++const_cast<uint32_t&>(dbg_full_render_counter);
    else
        ++const_cast<uint32_t&>(dbg_dirty_render_counter);
    // 更新调试信息
    wchar_t buffer[1024];
    std::swprintf(
        buffer, 1024,
        L"<%ls>: FRC: %d\nDRC: %d\nDRRC: %d",
        L"NAMELESS",
        int(dbg_full_render_counter),
        int(dbg_dirty_render_counter),
        int(0)
    );
    // 设置显示
    /*UIManager.RenderUnlock();
    this->dbg_uiwindow->SetTitleName(buffer);
    UIManager.RenderLock();*/
#endif
    return hr;
}



/// <summary>
/// Gets the system last error.
/// </summary>
/// <returns></returns>
auto LongUI::Result::GetSystemLastError() noexcept -> Result {
    const auto x = ::GetLastError();
    constexpr int32_t UI_FACILITY = 7;
    return{ ((int32_t)(x) <= 0 ? ((int32_t)(x)) :
        ((int32_t)(((x) & 0x0000FFFF) | (UI_FACILITY << 16) | 0x80000000))) };
}



#ifndef NDEBUG

void ui_dbg_set_window_title(
    LongUI::CUIWindow* pwnd,
    const wchar_t * title) noexcept {
    assert(pwnd && "bad action");
    pwnd->SetTitleName(title);
}

void ui_dbg_set_window_title(
    HWND hwnd,
    const wchar_t * title) noexcept {
    assert(hwnd && "bad action");
    ::SetWindowTextW(hwnd, title);
}

#endif
