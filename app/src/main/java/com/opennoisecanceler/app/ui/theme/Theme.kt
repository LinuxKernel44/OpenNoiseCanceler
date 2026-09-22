package com.opennoisecanceler.app.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

// The app is dark-theme-only by design (see project requirements): a
// real-time audio status/diagnostics tool is used in short glances, often
// in low-light conditions (headphones already on), and a single consistent
// theme keeps the "is ANC on?" signal unambiguous rather than depending on
// system theme state.
private val OncColorScheme = darkColorScheme(
    background = BackgroundDark,
    surface = SurfaceDark,
    surfaceVariant = SurfaceVariantDark,
    primary = AccentActive,
    error = AccentError,
    onBackground = TextPrimary,
    onSurface = TextPrimary,
    onSurfaceVariant = TextSecondary,
    onPrimary = BackgroundDark,
    onError = TextPrimary,
)

@Composable
fun OpenNoiseCancelerTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = OncColorScheme,
        typography = OncTypography,
        content = content,
    )
}
