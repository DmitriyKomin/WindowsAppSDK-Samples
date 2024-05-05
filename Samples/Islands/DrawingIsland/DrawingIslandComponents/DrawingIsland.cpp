﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "DrawingIsland.h"
#include "DrawingIsland.g.cpp"

#include "IslandFragmentRoot.h"
#include "NodeSimpleFragment.h"

namespace winrt::DrawingIslandComponents::implementation
{
    DrawingIsland::DrawingIsland(
        const winrt::Compositor& compositor)
    {
        m_output.Compositor = compositor;

        // Create the Compositor and the Content:
        // - The Bridge's connection to the Window will keep everything alive, and perform an
        //   orderly tear-down:
        //
        //   Window -> Bridge -> Content -> Visual -> InputSite -> InputObject
        //
        // - When the ContentIsland is destroyed, ContentIsland.AppData will call IClosable.Close on
        //   this instance to break cycles and clean up expensive resources.

        m_background.Visual = m_output.Compositor.CreateSpriteVisual();
        m_background.Visual.RelativeSizeAdjustment(float2(1.0f, 1.0f));

        m_island = winrt::ContentIsland::Create(m_background.Visual);
        m_island.AppData(get_strong().as<winrt::IInspectable>());

        Output_Initialize();
        Input_Initialize();
        Window_Initialize();
        Accessibility_Initialize();

        m_prevState.RasterizationScale = m_island.RasterizationScale();

        // Get notifications for resize, bridge changes, etc.
        (void)m_island.StateChanged(
            [&](auto&& ...)
        {
            return Island_OnStateChanged();
        });


        (void)m_island.Closed(
            [&]()
        {
            return Island_OnClosed();
        });

    }


    DrawingIsland::~DrawingIsland()
    {
        m_uia.FragmentRoot = nullptr;
        m_uia.FragmentFactory = nullptr;
        m_output.Compositor = nullptr;
    }


    void
    DrawingIsland::Close()
    {
        m_items.Visuals = nullptr;
        m_items.SelectedVisual = nullptr;
        m_background.BrushDefault = nullptr;
        m_background.BrushA = nullptr;
        m_background.BrushB = nullptr;
        m_background.BrushC = nullptr;
        m_background.Visual = nullptr;

        // TODO: Enable Mica on Win 11
#if FALSE
        // Destroy SystemBackdropController objects.
        if (m_backdropController != nullptr)
        {
            if (m_backdropConfiguration != nullptr)
            {
                // m_backdropConfiguration is only initialized for the DrawingIsland that owns the
                // SystemBackdropController.

                m_backdropController.Close();
                m_backdropController = nullptr;
                m_backdropConfiguration = nullptr;
            }
            else
            {
                // We're closing a DrawingIsland in a popup, not the owner DrawingIsland of the
                // SystemBackdropController.  Therefore, remove the current target from the
                // Controller.

                m_backdropController.RemoveSystemBackdropTarget(m_backdropTarget);
            }
        }
#endif

        // Destroy Content:
        // - This handles if the ContentIsland has already started destruction and is notifying the
        //   app.

        m_island.Close();
        m_island = nullptr;

        // TODO: Add the following test case in automated tests:
        // 1. We are recursively calling ContentIsland.Close while inside the ContentIsland.Closed
        // event.
        // 2. We are testing the potential final Release() of ContentIsland while inside the Closed
        // event.
    }


    IFACEMETHODIMP
    DrawingIsland::OnDirectMessage(
        IInputPreTranslateKeyboardSourceInterop* /*source*/,
        const MSG* msg,
        UINT keyboardModifiers,
        _Inout_ bool* handled)
    {
        *handled = false;

        // Alt+A Debug print to see the order of the PreTranslate callbacks
        if ((keyboardModifiers & FALT) &&
            (msg->message == WM_SYSKEYDOWN) &&
            (static_cast<winrt::Windows::System::VirtualKey>(msg->wParam) == winrt::VirtualKey::A))
        {

        }

        if (keyboardModifiers & FALT)
        {            
            *handled = Input_AcceleratorKeyActivated(
                static_cast<winrt::Windows::System::VirtualKey>(msg->wParam));
        }

        return S_OK;
    }


    IFACEMETHODIMP
    DrawingIsland::OnTreeMessage(
        IInputPreTranslateKeyboardSourceInterop* /*source*/,
        const MSG* msg,
        UINT keyboardModifiers,
        _Inout_ bool* handled)
    {
        // Alt+A Debug print to see the order of the PreTranslate callbacks
        if ((keyboardModifiers & FALT) &&
            (msg->message == WM_SYSKEYDOWN) &&
            (static_cast<winrt::Windows::System::VirtualKey>(msg->wParam) == winrt::VirtualKey::A))
        {

        }

        *handled = false;
        return S_OK;
    }


    void 
    DrawingIsland::EvaluateUseSystemBackdrop()
    {
        BYTE backgroundBrushOpacity = 0xFF;

        // TODO: Enable Mica on Win 11
#if FALSE
        // If we use system backdrops, it will be behind our background visual. Make sure we can
        // see through the background visual to reveal the system backdrop.
        // reveal the system backdrop underneath.
        if (m_useSystemBackdrop)
        {
            backgroundBrushOpacity = 0x30;
        }
#endif

        // Create the background parent Visual that the individual square will be added into.
        m_background.BrushDefault = m_output.Compositor.CreateColorBrush(
            winrt::Color{ backgroundBrushOpacity, 0x20, 0x20, 0x20 });

        m_background.BrushA = m_output.Compositor.CreateColorBrush(
            winrt::Color{ backgroundBrushOpacity, 0x99, 0x20, 0x20 });

        m_background.BrushB = m_output.Compositor.CreateColorBrush(
            winrt::Color{ backgroundBrushOpacity, 0x20, 0x99, 0x20 });

        m_background.BrushC = m_output.Compositor.CreateColorBrush(
            winrt::Color{ backgroundBrushOpacity, 0x20, 0x20, 0x99 });

        m_background.Visual.Brush(m_background.BrushDefault);
    }


    // TODO: Enable Mica on Win 11
#if FALSE
    void
    DrawingIsland::UseSystemBackdrop(
        boolean value)
    {
        if (m_useSystemBackdrop != value)
        {
            m_useSystemBackdrop = value;

            EvaluateUseSystemBackdrop();
        }
    }
#endif


    void
    DrawingIsland::Input_Initialize()
    {
        m_input.PointerSource = winrt::InputPointerSource::GetForIsland(m_island);

        m_input.PointerSource.PointerPressed(
            [this](winrt::InputPointerSource const&,
                   winrt::PointerEventArgs const& args)
            {
                auto currentPoint = args.CurrentPoint();
                auto properties = currentPoint.Properties();

                if (properties.IsLeftButtonPressed())
                {
                    Input_OnLeftButtonPressed(args);
                }
                else if (properties.IsRightButtonPressed())
                {
                    Input_OnRightButtonPressed(args);
                }
        });

        m_input.PointerSource.PointerMoved(
            [this](winrt::InputPointerSource const&,
                   winrt::PointerEventArgs const& args)
            {
                Input_OnPointerMoved(args);
            });

        m_input.PointerSource.PointerReleased(
            [&](auto&& ...)
            {
                Input_OnPointerReleased();
            });

        // Set up the keyboard source. We store this in a member variable so we can easily call 
        // TrySetFocus() in response to left clicks in the content later on.
        m_input.KeyboardSource = winrt::InputKeyboardSource::GetForIsland(m_island);

        m_input.KeyboardSource.KeyDown(
            [this](winrt::InputKeyboardSource const&,
                   winrt::KeyEventArgs const& args)
        {
            bool handled = Input_OnKeyDown(args.VirtualKey());

            // Mark the event as handled
            if (handled)
            {
                args.Handled(true);
            }
        });

        m_input.PretranslateSource = winrt::InputPreTranslateKeyboardSource::GetForIsland(m_island);

        m_input.PretranslateSource.as<
            Microsoft::UI::Input::IInputPreTranslateKeyboardSourceInterop>()->
                SetPreTranslateHandler(this);

        auto activationListener = winrt::InputActivationListener::GetForIsland(m_island);
        (void)activationListener.InputActivationChanged(
            [this, activationListener](
                winrt::InputActivationListener const&,
                winrt::InputActivationListenerActivationChangedEventArgs const&)
        {
            switch (activationListener.State())
            {
            case winrt::InputActivationState::Activated:
                m_background.Visual.Opacity(1.0f);
                break;

            default:
                m_background.Visual.Opacity(m_background.Opacity);
                break;
            }
        });

        m_input.FocusController = winrt::InputFocusController::GetForIsland(m_island);
        m_input.FocusController.NavigateFocusRequested(
            [this](winrt::InputFocusController const&, winrt::FocusNavigationRequestEventArgs const& args) {
                bool setFocus = m_input.FocusController.TrySetFocus();
                // Mark the event as handled
                if (setFocus)
                {
                    args.Result(winrt::FocusNavigationResult::Moved);
                }
            });
    }


    bool 
    DrawingIsland::Input_OnKeyDown(
        winrt::Windows::System::VirtualKey virtualKey)
    {
        bool handled = false;

        switch (virtualKey)
        {
            case winrt::VirtualKey::A:
            {
                m_background.Visual.Brush(m_background.BrushA);
                handled = true;
                break;
            }

            case winrt::VirtualKey::B:
            {
                m_background.Visual.Brush(m_background.BrushB);
                handled = true;
                break;
            }

            case winrt::VirtualKey::C:
            {
                m_background.Visual.Brush(m_background.BrushC);
                handled = true;
                break;
            }

            case winrt::VirtualKey::Space:
            {
                m_background.Visual.Brush(m_background.BrushDefault);
                break;
            }
                
            case winrt::Windows::System::VirtualKey::Number1:
            {
                m_output.CurrentColorIndex = 0;
                handled = true;
                break;
            }

            case winrt::Windows::System::VirtualKey::Number2:
            {
                m_output.CurrentColorIndex = 1;
                handled = true;
                break;
            }

            case winrt::Windows::System::VirtualKey::Number3:
            {
                m_output.CurrentColorIndex = 2;
                handled = true;
                break;
            }

            case winrt::Windows::System::VirtualKey::Number4:
            {
                m_output.CurrentColorIndex = 3;
                handled = true;
                break;
            }

            case winrt::Windows::System::VirtualKey::Delete:
            case winrt::Windows::System::VirtualKey::Escape:
            {
                m_items.Visuals.RemoveAll();
                handled = true;
                break;
            }

            case winrt::Windows::System::VirtualKey::Tab:
            {
                auto request = winrt::Microsoft::UI::Input::FocusNavigationRequest::Create(
                    winrt::Microsoft::UI::Input::FocusNavigationReason::First);
                m_input.FocusController.DepartFocus(request);
            }
        }

        Output_UpdateCurrentColorVisual();

        return handled;
    }

    bool 
    DrawingIsland::Input_AcceleratorKeyActivated(
        winrt::Windows::System::VirtualKey virtualKey)
    {
        bool handled = false;
        
        switch (virtualKey)
        {
            case winrt::VirtualKey::X:
            {
                m_background.Visual.Brush(m_background.BrushA);
                handled = true;
                break;
            }

            case winrt::VirtualKey::Y:
            {
                m_background.Visual.Brush(m_background.BrushB);
                handled = true;
                break;
            }

            case winrt::VirtualKey::Z:
            {
                m_background.Visual.Brush(m_background.BrushC);
                handled = true;
                break;
            }
        }

        return handled;
    }


    void
    DrawingIsland::Accessibility_Initialize()
    {
        m_uia.FragmentRoot = winrt::make_self<IslandFragmentRoot>(m_island);
        m_uia.FragmentRoot->SetName(L"Drawing Squares");

        m_uia.FragmentFactory = winrt::make_self<NodeSimpleFragmentFactory>();

        (void)m_island.AutomationProviderRequested(
            { this, &DrawingIsland::Accessibility_OnAutomationProviderRequested });
    }


    void
    DrawingIsland::Accessibility_OnAutomationProviderRequested(
        const winrt::ContentIsland& /*island*/,
        const winrt::ContentIslandAutomationProviderRequestedEventArgs& args)
    {
        IInspectable providerAsIInspectable;
        m_uia.FragmentRoot->QueryInterface(
            winrt::guid_of<IInspectable>(),
            winrt::put_abi(providerAsIInspectable));

        args.AutomationProvider(std::move(providerAsIInspectable));

        args.Handled(true);
    }


    winrt::Visual
    DrawingIsland::HitTestVisual(
        float2 const point)
    {
        winrt::Visual selectedVisual{ nullptr };
        for (winrt::Visual visual : m_items.Visuals)
        {
            winrt::float3 const offset = visual.Offset();
            float2 const size = visual.Size();

            if (point.x >= offset.x &&
                point.x < offset.x + size.x &&
                point.y >= offset.y &&
                point.y < offset.y + size.y)
            {
                selectedVisual = visual;
            }
        }

        return selectedVisual;
    }


    void
    DrawingIsland::Input_OnLeftButtonPressed(
        const winrt::PointerEventArgs& args)
    {
        // Left button manipulates the custom-drawn content
        float2 const point = args.CurrentPoint().Position();

        bool controlPressed = WI_IsFlagSet(
            args.KeyModifiers(),
            winrt::Windows::System::VirtualKeyModifiers::Control);

        OnLeftClick(point, controlPressed);
    }


    void
    DrawingIsland::Input_OnPointerReleased()
    {
        m_items.SelectedVisual = nullptr;
    }


    void
    DrawingIsland::LeftClickAndRelease(
        const float2 currentPoint)
    {
        OnLeftClick(currentPoint, false);
        Input_OnPointerReleased();
    }


    void
    DrawingIsland::RightClickAndRelease(
        const float2 currentPoint)
    {
        OnRightClick(currentPoint);
        Input_OnPointerReleased();
    }


    void
    DrawingIsland::OnLeftClick(
        const float2 point,
        bool controlPressed)
    {
        m_items.SelectedVisual = HitTestVisual(point);
        
        if (m_items.SelectedVisual)
        {
            winrt::float3 const offset = m_items.SelectedVisual.Offset();

            m_items.Offset.x = offset.x - point.x;
            m_items.Offset.y = offset.y - point.y;

            m_items.Visuals.Remove(m_items.SelectedVisual);
            m_items.Visuals.InsertAtTop(m_items.SelectedVisual);

            // TODO: The m_uia.FragmentRoots child should be removed and added to the end as well.
        }
        else
        {
            Output_AddVisual(point, controlPressed);
        }
    }


    void
    DrawingIsland::Input_OnRightButtonPressed(
        const winrt::PointerEventArgs& args)
    {
        OnRightClick(args.CurrentPoint().Position());
    }

    void
    DrawingIsland::OnRightClick(const float2 point)
    {
        winrt::Visual selectedVisual = HitTestVisual(point);
    }

    void
    DrawingIsland::Input_OnPointerMoved(
        const winrt::PointerEventArgs& args)
    {
        if (m_items.SelectedVisual)
        {
            float2 const point = args.CurrentPoint().Position();

            m_items.SelectedVisual.Offset(
                { point.x + m_items.Offset.x,
                point.y + m_items.Offset.y,
                0.0f });
        }
    }


    void
    DrawingIsland::Island_OnStateChanged()
    {
        if (m_prevState.RasterizationScale != m_island.RasterizationScale())
        {
            m_prevState.RasterizationScale = m_island.RasterizationScale();
        }

        if (m_prevState.LayoutDirection != m_island.LayoutDirection())
        {
            SetLayoutDirectionForVisuals();
        }

        Output_UpdateCurrentColorVisual();
    }


    void
    DrawingIsland::SetLayoutDirectionForVisuals()
    {
        if (m_island.LayoutDirection() == ContentLayoutDirection::RightToLeft)
        {
            // The following will mirror the visuals. If any text is inside the visuals the text is
            // also mirrored. The text will need another RelativeOffsetAdjustment and Scale to
            // return to the normal space.

            m_background.Visual.RelativeOffsetAdjustment(winrt::float3(1, 0, 0));
            m_background.Visual.Scale(winrt::float3(-1, 1, 1));
        }
        else
        {
            m_background.Visual.RelativeOffsetAdjustment(winrt::float3(0, 0, 0));
            m_background.Visual.Scale(winrt::float3(1, 1, 1));
        }
        m_prevState.LayoutDirection = m_island.LayoutDirection();
    }


    void
    DrawingIsland::Island_OnClosed()
    {

    }


    void
    DrawingIsland::Output_Initialize()
    {
        for (int i = 0; i < _countof(m_output.ColorBrushes); i++)
        {
            m_output.ColorBrushes[i] = m_output.Compositor.CreateColorBrush(s_colors[i]);

            winrt::Color halfTransparent = s_colors[i];
            halfTransparent.A = 0x80;
            m_output.HalfTransparentColorBrushes[i] = m_output.Compositor.CreateColorBrush(halfTransparent);
        }

        m_items.CurrentColorVisual = m_output.Compositor.CreateSpriteVisual();
        m_items.CurrentColorVisual.Offset({0.0f, 0.0f, 0.0f});
        m_background.Visual.Children().InsertAtTop(m_items.CurrentColorVisual);

        winrt::ContainerVisual drawingVisualsRoot = m_output.Compositor.CreateContainerVisual();
        m_items.Visuals = drawingVisualsRoot.Children();
        m_background.Visual.Children().InsertAtTop(drawingVisualsRoot);

        EvaluateUseSystemBackdrop();

        Output_UpdateCurrentColorVisual();
    }


    void
    DrawingIsland::Output_AddVisual(
        float2 const point,
        bool halfTransparent)
    {
        winrt::SpriteVisual visual = m_output.Compositor.CreateSpriteVisual();
        visual.Brush(halfTransparent ? 
            m_output.HalfTransparentColorBrushes[m_output.CurrentColorIndex] : 
            m_output.ColorBrushes[m_output.CurrentColorIndex]);

        float const BlockSize = 30.0f;
        visual.Size({ BlockSize, BlockSize });
        visual.Offset({ point.x - BlockSize / 2.0f, point.y - BlockSize / 2.0f, 0.0f });

        m_items.Visuals.InsertAtTop(visual);

        m_items.SelectedVisual = visual;
        m_items.Offset.x = -BlockSize / 2.0f;
        m_items.Offset.y = -BlockSize / 2.0f;

        CreateUIAProviderForVisual();

        Accessibility_UpdateScreenCoordinates(m_items.SelectedVisual);
    }


    void
    DrawingIsland::Accessibility_UpdateScreenCoordinates(
        const winrt::Visual & visual)
    {
        winrt::Rect logicalRect;
        logicalRect.X = visual.Offset().x;
        logicalRect.Y = visual.Offset().y;
        logicalRect.Width = visual.Size().x;
        logicalRect.Height = visual.Size().y;

        auto fragment = m_uia.VisualToFragmentMap[visual];

        // This will convert from the logical coordinate space of the ContentIsland to
        // Win32 screen coordinates that are needed by Accesibility.
        auto screenRect = m_island.CoordinateConverter().ConvertLocalToScreen(logicalRect);

        UiaRect uiaRect;
        uiaRect.left = screenRect.X;
        uiaRect.top = screenRect.Y;
        uiaRect.width = screenRect.Width;
        uiaRect.height = screenRect.Height;
        
        fragment->SetBoundingRects(uiaRect);
    }


    void
    DrawingIsland::CreateUIAProviderForVisual()
    {
        winrt::com_ptr<NodeSimpleFragment> fragment = m_uia.FragmentFactory->Create(
            s_colorNames[m_output.CurrentColorIndex].c_str(), m_uia.FragmentRoot);

        m_uia.VisualToFragmentMap[m_items.SelectedVisual] = fragment;

        fragment->SetVisual(m_items.SelectedVisual);
        // Set up children roots
        m_uia.FragmentRoot->AddChild(fragment);

        // Finally set up parent chain
        fragment->SetParent(m_uia.FragmentRoot);
    }


    void 
    DrawingIsland::Output_UpdateCurrentColorVisual()
    {
        m_items.CurrentColorVisual.Brush(m_output.ColorBrushes[m_output.CurrentColorIndex]);
        m_items.CurrentColorVisual.Offset({0.0f, m_island.ActualSize().y - 25.0f, 0.0f});
        m_items.CurrentColorVisual.Size({m_island.ActualSize().x, 25.0f});
    }


    // TODO: Enable Mica on Win 11
#if FALSE
    void
    DrawingIsland::SystemBackdrop_Initialize()
    {
        // Don't initilize system backdrop if we haven't been configured for it.
        if (!m_useSystemBackdrop) return;

        if (m_backdropController == nullptr)
        {
            m_backdropConfiguration = winrt::SystemBackdropConfiguration();
            m_backdropController = winrt::DesktopAcrylicController();
            m_backdropController.SetSystemBackdropConfiguration(m_backdropConfiguration);

            auto activationListener = winrt::InputActivationListener::GetForIsland(m_island);
            (void)activationListener.InputActivationChanged(
                [this, activationListener](
                    winrt::InputActivationListener const&,
                    winrt::InputActivationListenerActivationChangedEventArgs const&)
            {
                switch (activationListener.State())
                {
                case winrt::InputActivationState::Activated:
                    m_backdropConfiguration.IsInputActive(true);
                    break;

                default:
                    m_backdropConfiguration.IsInputActive(false);
                    break;
                }
            });
        }


        // If we are the main content, we don't want to add custom clips or offsets to our 
        // backdrop, so we can pass the ContentIsland as the target to the BackdropController. 
        // This will by default fill the entire ContentIsland backdrop surface.

        m_backdropTarget = m_island;
    
        m_backdropController.AddSystemBackdropTarget(m_backdropTarget);
    }
#endif

    void
    DrawingIsland::Window_Initialize()
    {
        auto window = m_island.Environment();

        (void)window.SettingChanged(
            [this](winrt::ContentIslandEnvironment const&, winrt::ContentEnvironmentSettingChangedEventArgs const& args)
        {
            return Window_OnSettingChanged(args);
        });

        (void)window.StateChanged(
            [this](winrt::ContentIslandEnvironment const& sender, winrt::IInspectable const&)
        {
            return Window_OnStateChanged(sender);
        });
    }


    void
    DrawingIsland::Window_OnSettingChanged(
        const winrt::ContentEnvironmentSettingChangedEventArgs& args)
    {
        auto settingChanged = args.SettingName();

        if (settingChanged == L"intl")
        {
            m_background.Visual.Brush(m_background.BrushA);
        }
    }


    void
    DrawingIsland::Window_OnStateChanged(winrt::ContentIslandEnvironment const& /*sender*/)
    {

    }
}  // namespace winrt::OneTest::ContentIslandHelpers::implementation
