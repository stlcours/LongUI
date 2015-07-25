﻿#pragma once
/**
* Copyright (c) 2014-2015 dustpg   mailto:dustpg@gmail.com
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/


// LongUI namespace
namespace LongUI {
    /// <summary>
    /// marginal-able control
    /// </summary>
    /// <remarks>
    /// like: hamburger menu, menu bar, tool bar, tab bar, scroll bar, barabara, etc
    /// </remarks>
    class LongUIAlignas UIMarginalable : public UIControl {
        // 父类声明
        using Super = UIControl;
    public:
        /// <summary>
        /// position of marginal control
        /// </summary>
        enum MarginalControl : uint16_t {
            Control_Unknown= 0xFFFFui16,// unknown
            Control_Left = 0,           // control at left
            Control_Top,                // control at top
            Control_Right,              // control at right
            Control_Bottom,             // control at bottom
            MARGINAL_CONTROL_SIZE       // control-size
        };
        /// <summary>
        /// rule for handling overlapping problem in cross area
        /// </summary>
        enum CrossAreaRule : uint16_t {
            Rule_Greedy = 0,            // menubar like
            Rule_Generous,              // scrollbar like
        };

        /// <summary>
        /// Initializes the marginal control.
        /// </summary>
        /// <param name="_type" type="enum LongUI::UIMarginalable::MarginalControl">
        /// The type of MarginalControl.
        /// </param>
        /// <remarks>
        /// if a 'marginal-able' control as a 'marginal' control, this method 
        /// called before Event::Event_TreeBulidingFinished message.
        /// more detail to see <see cref="LongUI::UIContainer::DoEvent"/>
        /// 
        /// 如果一个可边缘化的控件作为边缘控件时, 这个方法在收到
        /// Event::Event_TreeBulidingFinished 消息前被调用, 
        /// 更多细节参考<see cref="LongUI::UIContainer::DoEvent"/>
        /// </remarks>
        /// <returns>void</returns>
        virtual inline void InitMarginalControl(MarginalControl _type) noexcept { force_cast(this->marginal_type) = _type; }

/*// <summary>
/// Test the cross area for other 2 adjacent controls
/// </summary>
/// <param name="area" type="float[2]">[IN] [OUT]The 2 cross areas</param>
/// <remarks>
/// in this method, you should do 2 steps:
/// 1.  you will get 2 areas from other 2 adjacent controls, get 0.f
///     if no control in one side, you should return the areas you want
///     to take, for example, for scrollbar, you should just return them
///     intactly to be "generous", because 2 scrollbars cannot cross each
///     other, for menu bar, you should return [0.f, 0.f] to be "greedy",
/// 2.  adjust another "width" size:
///     for example: 
///     "LEFT" marginal control, parent sends param "area" to adjust
///       your "height", but your "width" should be managed by yourself
///     "TOP" marginal control, parent sends param "area"  to adjust
///       your "width", but your "height" should be managed by yourself
/// </remarks>
/// <returns>void</returns>
        virtual void CrossAreaTest(float IN OUT area[]) noexcept { UNREFERENCED_PARAMETER(area); }*/

        // refresh the world transform while in marginal
        void RefreshWorldMarginal() noexcept;
    public:
        // marginal width, parent's real margin(ltrb) = parent's original(ltrb) + this' marginal_width
        // example: classic scrollbar's marginal_width = it's width/height
        //          modern scrollbar(show when mouse pointed)'s marginal_width = 0
        float                   marginal_width = 0.f;
        // rule for cross area
        CrossAreaRule     const rule = CrossAreaRule::Rule_Generous;
        // marginal type
        MarginalControl   const marginal_type = MarginalControl::Control_Unknown;
    protected:
        // ctor
        UIMarginalable(pugi::xml_node node) noexcept : Super(node) {}
#ifdef LongUIDebugEvent
    protected:
        // debug infomation
        virtual bool debug_do_event(const LongUI::DebugEventInformation&) const noexcept override;
#endif
    };
#ifdef LongUIDebugEvent
    // 重载?特例化 GetIID
    template<> LongUIInline const IID& GetIID<LongUI::UIMarginalable>() {
        // {6CF3853D-6740-4635-AF7E-F8A42AEBA6C9}
        static const GUID IID_LongUI_UIMarginalControl = { 
            0x6cf3853d, 0x6740, 0x4635,{ 0xaf, 0x7e, 0xf8, 0xa4, 0x2a, 0xeb, 0xa6, 0xc9 } 
        };
        return IID_LongUI_UIMarginalControl;
    }
#endif
}