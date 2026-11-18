use proc_macro::TokenStream;
use quote::quote;
use syn::{Attribute, Expr, Ident, Item, Lit, LitStr, Meta, MetaNameValue};

/// Annotations understood by the RustAnnotationBridge LLVM pass.
const ANNOTATIONS: &[&str] = &["to_harden", "to_duplicate", "exclude"];

/// Annotates a function or a static with an ASPIS annotation, e.g. `#[aspis(to_harden)]`.
///
/// The annotations are mutually exclusive, and the ASPIS passes only honor the first
/// annotation of a symbol, so exactly one must be given.
///
/// The annotation is encoded in the item's link section as an `aspis_<annotation>` entry,
/// which the RustAnnotationBridge LLVM pass turns into an `llvm.global.annotations` entry
/// and strips from the section. The encoding is only emitted when compiling with
/// `--cfg aspis`, so a regular compilation produces the same output as the unannotated code.
///
/// A `link_section` already on the item is kept in both cases. Since the macro only sees the
/// attributes below it, `#[aspis(...)]` must be placed above the `link_section` attribute:
///
/// ```ignore
/// #[aspis(to_harden)]
/// #[unsafe(link_section = ".mysec")]
/// static mut X: i32 = 0;
/// ```
#[proc_macro_attribute]
pub fn aspis(attr: TokenStream, item: TokenStream) -> TokenStream {
    match expand(attr.into(), item.into()) {
        Ok(tokens) => tokens.into(),
        Err(e) => e.to_compile_error().into(),
    }
}

fn expand(
    attr: proc_macro2::TokenStream,
    item: proc_macro2::TokenStream,
) -> syn::Result<proc_macro2::TokenStream> {
    let annotation = parse_aspis_annotation(attr)?;

    let mut parsed: Item = syn::parse2(item)?;
    let attrs = match &mut parsed {
        Item::Fn(item) => &mut item.attrs,
        Item::Static(item) => &mut item.attrs,
        _ => {
            return Err(syn::Error::new_spanned(
                parsed,
                "#[aspis] only supports functions and statics",
            ));
        }
    };

    // Take the already present, if present, link_section attribute
    let section = take_link_section(attrs)?;

    // Prepare the content of the new ASPIS-compatible link_section attribute
    let mut entries: Vec<String> = section.iter().map(LitStr::value).collect();
    entries.push(format!("aspis_{annotation}"));
    let aspis_section = LitStr::new(&entries.join(","), proc_macro2::Span::call_site());

    // Wrap the already present, if present, link_section with a cfg_attr(not(aspis))
    // so that "not-instrumented" code does not have anything ASPIS-related
    let plain = section.map(|section| {
        quote! { #[cfg_attr(not(aspis), unsafe(link_section = #section))] }
    });

    // Wrap the aspis_section with a cfg_attr(aspis) so that it can be enabled at will
    Ok(quote! {
        #[cfg_attr(aspis, unsafe(link_section = #aspis_section))]
        #plain
        #parsed
    })
}

fn parse_aspis_annotation(attr: proc_macro2::TokenStream) -> syn::Result<Ident> {
    let expected = format!("expected one of: {}", ANNOTATIONS.join(", "));
    if attr.is_empty() {
        return Err(syn::Error::new(
            proc_macro2::Span::call_site(),
            format!("missing ASPIS annotation, {expected}"),
        ));
    }
    let annotation: Ident = syn::parse2(attr).map_err(|e| {
        syn::Error::new(
            e.span(),
            format!("expected a single ASPIS annotation, {expected}"),
        )
    })?;
    if !ANNOTATIONS.contains(&annotation.to_string().as_str()) {
        return Err(syn::Error::new_spanned(
            annotation,
            format!("unknown ASPIS annotation, {expected}"),
        ));
    }
    Ok(annotation)
}

/// Removes the `#[unsafe(link_section = "...")]` attribute from `attrs`, returning its section
/// name. Only edition 2024 sources are supported, so the unsafe-less `#[link_section = "..."]`
/// form is rejected.
fn take_link_section(attrs: &mut Vec<Attribute>) -> syn::Result<Option<LitStr>> {
    let mut section = None;
    let mut error: Option<syn::Error> = None;

    attrs.retain(|attr| {
        let name_value = match &attr.meta {
            Meta::List(list) if list.path.is_ident("unsafe") => {
                match list.parse_args::<MetaNameValue>() {
                    Ok(nv) if nv.path.is_ident("link_section") => nv,
                    _ => return true,
                }
            }
            meta if meta.path().is_ident("link_section") => {
                let e = syn::Error::new_spanned(
                    attr,
                    "link_section must be written as `#[unsafe(link_section = \"...\")]`, \
                     only edition 2024 sources are supported",
                );
                match &mut error {
                    Some(error) => error.combine(e),
                    None => error = Some(e),
                }
                return false;
            }
            _ => return true,
        };

        let result = match &name_value.value {
            Expr::Lit(lit) => match &lit.lit {
                Lit::Str(s) if section.is_none() => Ok(s.clone()),
                Lit::Str(_) => Err(syn::Error::new_spanned(attr, "duplicate link_section")),
                _ => Err(syn::Error::new_spanned(
                    &name_value.value,
                    "expected a string literal",
                )),
            },
            _ => Err(syn::Error::new_spanned(
                &name_value.value,
                "expected a string literal",
            )),
        };
        match result {
            Ok(s) => section = Some(s),
            Err(e) => match &mut error {
                Some(error) => error.combine(e),
                None => error = Some(e),
            },
        }
        false
    });

    match error {
        Some(e) => Err(e),
        None => Ok(section),
    }
}
