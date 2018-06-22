//! Shared access to Cairo image surfaces.
use std::ptr::NonNull;

use cairo::prelude::SurfaceExt;
use cairo::{self, ImageSurface};
use cairo_sys;
use glib::translate::{Stash, ToGlibPtr};

use filters::context::IRect;

use super::Pixel;

/// Wrapper for a Cairo image surface that allows shared access.
///
/// There doesn't seem to be any good way of making safe shared access to `ImageSurface` pixel
/// data, since a read-only borrowed reference can still be cloned and then modified (for example,
/// via a `Context`). We can't simply use `ImageSurface::get_data()` because in the filter code we
/// have surfaces referenced from multiple places and it would probably add more complexity to
/// remove that and start passing around references.
///
/// This wrapper asserts the uniqueness of its image surface and doesn't permit modifying it.
///
/// Note: originally I had an idea of using `Rc<RefCell<ImageSurface>>` here which allows to create
/// both read-only and unique read-write accessors safely, however then I realized a read-write
/// accessor isn't of much use if it can't expose a Cairo context interface. Cairo contexts have
/// the very same issue that they can be cloned from a read-only reference and break all safety
/// constraints in this way. Thus the only safe way of exposing a Cairo context seemed to be to
/// manually add all Cairo context methods on the accessor forwarding to the underlying Cairo
/// context (without exposing the context itself to prevent cloning), which would result in too
/// much code. Unless it's absolutely required, I'd like to avoid that.
///
/// Having just read-only access simplifies things further dropping the need for `Rc<RefCell<>>`
/// altogether.
#[derive(Debug, Clone)]
pub struct SharedImageSurface {
    surface: ImageSurface,

    data_ptr: NonNull<u8>, // *const.
    width: i32,
    height: i32,
    stride: isize,
}

impl SharedImageSurface {
    /// Creates a `SharedImageSurface` from a unique `ImageSurface`.
    ///
    /// # Panics
    /// Panics if the `ImageSurface` is not unique, that is, its reference count isn't 1.
    #[inline]
    pub fn new(surface: ImageSurface) -> Result<Self, cairo::Status> {
        let reference_count =
            unsafe { cairo_sys::cairo_surface_get_reference_count(surface.to_raw_none()) };
        assert_eq!(reference_count, 1);

        surface.flush();
        if surface.status() != cairo::Status::Success {
            return Err(surface.status());
        }

        let data_ptr = NonNull::new(unsafe {
            cairo_sys::cairo_image_surface_get_data(surface.to_raw_none())
        }).unwrap();

        let width = surface.get_width();
        let height = surface.get_height();
        let stride = surface.get_stride() as isize;

        Ok(Self {
            surface,
            data_ptr,
            width,
            height,
            stride,
        })
    }

    /// Converts this `SharedImageSurface` back into a Cairo image surface.
    ///
    /// # Panics
    /// Panics if the underlying Cairo image surface is not unique, that is, there are other
    /// instances of `SharedImageSurface` pointing at the same Cairo image surface.
    #[inline]
    pub fn into_image_surface(self) -> ImageSurface {
        let reference_count =
            unsafe { cairo_sys::cairo_surface_get_reference_count(self.surface.to_raw_none()) };
        assert_eq!(reference_count, 1);

        self.surface
    }

    /// Returns the surface width.
    #[inline]
    pub fn width(&self) -> i32 {
        self.width
    }

    /// Returns the surface height.
    #[inline]
    pub fn height(&self) -> i32 {
        self.height
    }

    /// Retrieves the pixel value at the given coordinates.
    #[inline]
    pub fn get_pixel(&self, x: u32, y: u32) -> Pixel {
        assert!(x < self.width as u32);
        assert!(y < self.height as u32);

        let value = unsafe {
            *(self
                .data_ptr
                .as_ptr()
                .offset(y as isize * self.stride + x as isize * 4) as *const u32)
        };

        Pixel {
            r: ((value >> 16) & 0xFF) as u8,
            g: ((value >> 8) & 0xFF) as u8,
            b: (value & 0xFF) as u8,
            a: ((value >> 24) & 0xFF) as u8,
        }
    }

    /// Calls `set_source_surface()` on the given Cairo context.
    #[inline]
    pub fn set_as_source_surface(&self, cr: &cairo::Context, x: f64, y: f64) {
        cr.set_source_surface(&self.surface, x, y);
    }

    /// Returns a new `ImageSurface` with the same contents as the one stored in this
    /// `SharedImageSurface` within the given bounds.
    pub fn copy_surface(&self, bounds: IRect) -> Result<ImageSurface, cairo::Status> {
        let output_surface = ImageSurface::create(cairo::Format::ARgb32, self.width, self.height)?;

        let cr = cairo::Context::new(&output_surface);
        cr.rectangle(
            bounds.x0 as f64,
            bounds.y0 as f64,
            (bounds.x1 - bounds.x0) as f64,
            (bounds.y1 - bounds.y0) as f64,
        );
        cr.clip();

        cr.set_source_surface(&self.surface, 0f64, 0f64);
        cr.paint();

        Ok(output_surface)
    }

    /// Returns a raw pointer to the underlying surface.
    ///
    /// # Safety
    /// The returned pointer must not be used to modify the surface.
    #[inline]
    pub unsafe fn to_glib_none(&self) -> Stash<*mut cairo_sys::cairo_surface_t, ImageSurface> {
        self.surface.to_glib_none()
    }
}