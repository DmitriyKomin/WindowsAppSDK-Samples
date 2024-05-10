﻿// // Copyright (c) Microsoft. All rights reserved.
// // Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.Windows;
using System.Windows.Interop;

#if true // Demo3_Step2_AddCompact

using Microsoft.UI;
using Microsoft.UI.Windowing;

namespace CalculatorDemo
{
    public sealed partial class MainWindow : Window
    {
        private AppWindow _appWindow;

        private void CompactView_Click(object sender, RoutedEventArgs e)
        {
            SetCompactView(true);
        }

        private void ExitCompactViewButton_Click(object sender, RoutedEventArgs e)
        {
            SetCompactView(false);
        }

        void SetCompactView(bool useCompactView)
        {
            // Ensure we have an AppWindow for this WPF Window.
            if (_appWindow == null)
            {
                _appWindow = AppWindow.GetFromWindowId(
                    new WindowId((ulong)new WindowInteropHelper(this).Handle));
            }

            if (useCompactView)
            {
                // For compact view, hide the main panel and show the compact panel.
                MyPanel.Visibility = Visibility.Collapsed;
                CompactPanel.Visibility = Visibility.Visible;

                CompactViewText.Text = DisplayBox.Text;

                // The AppWindow's CompactOverlay mode will make it always-on-top.
                _appWindow.SetPresenter(AppWindowPresenterKind.CompactOverlay);
            }
            else
            {
                MyPanel.Visibility = Visibility.Visible;
                CompactPanel.Visibility = Visibility.Collapsed;

                _appWindow.SetPresenter(AppWindowPresenterKind.Default);
            }
        }
   }
}

#endif