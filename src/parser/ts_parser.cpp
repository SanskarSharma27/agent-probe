#include "parser/ts_parser.h"

#include <fstream>
#include <sstream>
#include <cstring>

namespace probe {

TSParserWrapper::TSParserWrapper() : parser_(ts_parser_new()) {}

TSParserWrapper::~TSParserWrapper() {
    if (parser_) ts_parser_delete(parser_);
}

bool TSParserWrapper::set_language(const TSLanguage* language) {
    return ts_parser_set_language(parser_, language);
}

std::vector<ASTNode> TSParserWrapper::parse_file(const std::string& file_path,
                                                  const LanguageProfile& profile) {
    std::ifstream file(file_path);
    if (!file.is_open()) return {};

    std::stringstream buf;
    buf << file.rdbuf();
    return parse_string(buf.str(), file_path, profile);
}

std::vector<ASTNode> TSParserWrapper::parse_string(const std::string& source,
                                                    const std::string& file_path,
                                                    const LanguageProfile& profile) {
    TSTree* tree = ts_parser_parse_string(parser_, nullptr, source.c_str(), source.size());
    if (!tree) return {};

    TSNode root = ts_tree_root_node(tree);
    std::vector<ASTNode> results;

    extract_imports(root, source, file_path, profile, results);
    extract_classes(root, source, file_path, profile, results);
    extract_functions(root, source, file_path, profile, results);

    ts_tree_delete(tree);
    return results;
}

std::string TSParserWrapper::node_text(TSNode node, const std::string& source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    if (start >= source.size() || end > source.size()) return "";
    return source.substr(start, end - start);
}

std::string TSParserWrapper::extract_call_name(TSNode call_node, const std::string& source,
                                                const LanguageProfile& profile) {
    // The "function" field of a call node is either:
    //   - an identifier: foo()
    //   - an attribute: requests.get()
    TSNode func = ts_node_child_by_field_name(
        call_node, profile.field_function().c_str(), profile.field_function().size()
    );

    if (ts_node_is_null(func)) return "";

    const char* type = ts_node_type(func);

    if (std::strcmp(type, profile.attribute_type().c_str()) == 0) {
        // attribute node: object.attribute → "object.attribute"
        TSNode obj = ts_node_child_by_field_name(
            func, profile.field_object().c_str(), profile.field_object().size()
        );
        TSNode attr = ts_node_child_by_field_name(
            func, profile.field_attribute().c_str(), profile.field_attribute().size()
        );
        if (!ts_node_is_null(obj) && !ts_node_is_null(attr)) {
            return node_text(obj, source) + "." + node_text(attr, source);
        }
    }

    // Simple identifier or fallback
    return node_text(func, source);
}

void TSParserWrapper::collect_calls(TSNode node, const std::string& source,
                                     const LanguageProfile& profile,
                                     std::vector<std::string>& calls) {
    const char* type = ts_node_type(node);

    if (std::strcmp(type, profile.call_expression_type().c_str()) == 0) {
        std::string name = extract_call_name(node, source, profile);
        if (!name.empty()) {
            calls.push_back(name);
        }
    }

    // Recurse into children
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        collect_calls(ts_node_child(node, i), source, profile, calls);
    }
}

std::vector<std::string> TSParserWrapper::extract_parameters(TSNode params_node,
                                                              const std::string& source) {
    std::vector<std::string> params;
    uint32_t count = ts_node_named_child_count(params_node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_named_child(params_node, i);
        const char* type = ts_node_type(child);

        if (std::strcmp(type, "identifier") == 0) {
            params.push_back(node_text(child, source));
        } else if (std::strcmp(type, "default_parameter") == 0 ||
                   std::strcmp(type, "typed_parameter") == 0 ||
                   std::strcmp(type, "typed_default_parameter") == 0) {
            // Extract just the parameter name (first named child)
            TSNode name_node = ts_node_child_by_field_name(child, "name", 4);
            if (!ts_node_is_null(name_node)) {
                params.push_back(node_text(name_node, source));
            } else if (ts_node_named_child_count(child) > 0) {
                params.push_back(node_text(ts_node_named_child(child, 0), source));
            }
        } else if (std::strcmp(type, "list_splat_pattern") == 0 ||
                   std::strcmp(type, "dictionary_splat_pattern") == 0) {
            // *args, **kwargs
            if (ts_node_named_child_count(child) > 0) {
                std::string pname = node_text(ts_node_named_child(child, 0), source);
                params.push_back(pname);
            }
        }
    }
    return params;
}

std::vector<std::string> TSParserWrapper::extract_decorators(TSNode decorated_node,
                                                              const std::string& source,
                                                              const LanguageProfile& profile) {
    std::vector<std::string> decorators;
    uint32_t count = ts_node_child_count(decorated_node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(decorated_node, i);
        if (std::strcmp(ts_node_type(child), profile.decorator_type().c_str()) == 0) {
            // The decorator text includes '@', strip it
            std::string text = node_text(child, source);
            if (!text.empty() && text[0] == '@') {
                text = text.substr(1);
            }
            // Trim to just the name/attribute (remove arguments if present)
            auto paren = text.find('(');
            if (paren != std::string::npos) {
                text = text.substr(0, paren);
            }
            decorators.push_back(text);
        }
    }
    return decorators;
}

void TSParserWrapper::extract_imports(TSNode node, const std::string& source,
                                       const std::string& file_path,
                                       const LanguageProfile& profile,
                                       std::vector<ASTNode>& results) {
    const char* type = ts_node_type(node);

    // "import X" or "import X, Y"
    if (std::strcmp(type, profile.import_statement_type().c_str()) == 0) {
        ASTNode imp;
        imp.type = NodeType::IMPORT;
        imp.file_path = file_path;
        imp.start_line = ts_node_start_point(node).row + 1;
        imp.end_line = ts_node_end_point(node).row + 1;

        // Collect all dotted_name children as imported modules
        uint32_t count = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_named_child(node, i);
            const char* child_type = ts_node_type(child);
            if (std::strcmp(child_type, "dotted_name") == 0 ||
                std::strcmp(child_type, "aliased_import") == 0) {
                std::string mod = node_text(child, source);
                // For aliased imports like "import X as Y", just take the module name
                if (std::strcmp(child_type, "aliased_import") == 0) {
                    TSNode name = ts_node_named_child(child, 0);
                    if (!ts_node_is_null(name)) mod = node_text(name, source);
                }
                if (imp.module.empty()) {
                    imp.module = mod;
                    imp.name = mod;
                } else {
                    imp.imported_names.push_back(mod);
                }
            }
        }
        results.push_back(std::move(imp));
        return;
    }

    // "from X import Y, Z"
    if (std::strcmp(type, profile.import_from_type().c_str()) == 0) {
        ASTNode imp;
        imp.type = NodeType::IMPORT;
        imp.file_path = file_path;
        imp.start_line = ts_node_start_point(node).row + 1;
        imp.end_line = ts_node_end_point(node).row + 1;

        TSNode mod_node = ts_node_child_by_field_name(
            node, profile.field_module_name().c_str(), profile.field_module_name().size()
        );
        if (!ts_node_is_null(mod_node)) {
            imp.module = node_text(mod_node, source);
            imp.name = imp.module;
        }

        // Collect imported names from the import list
        uint32_t count = ts_node_named_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_named_child(node, i);
            const char* child_type = ts_node_type(child);

            if (std::strcmp(child_type, "dotted_name") == 0 &&
                node_text(child, source) != imp.module) {
                imp.imported_names.push_back(node_text(child, source));
            } else if (std::strcmp(child_type, "aliased_import") == 0) {
                TSNode name = ts_node_named_child(child, 0);
                if (!ts_node_is_null(name)) {
                    imp.imported_names.push_back(node_text(name, source));
                }
            }
        }
        results.push_back(std::move(imp));
        return;
    }

    // Recurse into top-level children only
    if (std::strcmp(type, "module") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            extract_imports(ts_node_child(node, i), source, file_path, profile, results);
        }
    }
}

void TSParserWrapper::extract_classes(TSNode node, const std::string& source,
                                       const std::string& file_path,
                                       const LanguageProfile& profile,
                                       std::vector<ASTNode>& results) {
    const char* type = ts_node_type(node);

    // Handle decorated classes
    if (std::strcmp(type, "decorated_definition") == 0) {
        std::vector<std::string> decorators = extract_decorators(node, source, profile);
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            if (std::strcmp(ts_node_type(child), profile.class_def_type().c_str()) == 0) {
                // Build class node with decorators
                ASTNode cls;
                cls.type = NodeType::CLASS_DEF;
                cls.file_path = file_path;
                cls.start_line = ts_node_start_point(child).row + 1;
                cls.end_line = ts_node_end_point(child).row + 1;
                cls.decorators = decorators;

                TSNode name_node = ts_node_child_by_field_name(
                    child, profile.field_name().c_str(), profile.field_name().size()
                );
                if (!ts_node_is_null(name_node)) {
                    cls.name = node_text(name_node, source);
                }

                // Extract base classes from superclasses/argument_list
                TSNode supers = ts_node_child_by_field_name(child, "superclasses", 12);
                if (!ts_node_is_null(supers)) {
                    uint32_t sc = ts_node_named_child_count(supers);
                    for (uint32_t j = 0; j < sc; j++) {
                        cls.base_classes.push_back(
                            node_text(ts_node_named_child(supers, j), source)
                        );
                    }
                }

                // Extract method names from class body
                TSNode body = ts_node_child_by_field_name(
                    child, profile.field_body().c_str(), profile.field_body().size()
                );
                if (!ts_node_is_null(body)) {
                    uint32_t bc = ts_node_child_count(body);
                    for (uint32_t j = 0; j < bc; j++) {
                        TSNode body_child = ts_node_child(body, j);
                        const char* bc_type = ts_node_type(body_child);
                        if (std::strcmp(bc_type, profile.function_def_type().c_str()) == 0) {
                            TSNode mname = ts_node_child_by_field_name(
                                body_child, profile.field_name().c_str(), profile.field_name().size()
                            );
                            if (!ts_node_is_null(mname)) {
                                cls.methods.push_back(node_text(mname, source));
                            }
                        } else if (std::strcmp(bc_type, "decorated_definition") == 0) {
                            // Decorated method
                            uint32_t dc = ts_node_child_count(body_child);
                            for (uint32_t k = 0; k < dc; k++) {
                                TSNode dd_child = ts_node_child(body_child, k);
                                if (std::strcmp(ts_node_type(dd_child), profile.function_def_type().c_str()) == 0) {
                                    TSNode mname = ts_node_child_by_field_name(
                                        dd_child, profile.field_name().c_str(), profile.field_name().size()
                                    );
                                    if (!ts_node_is_null(mname)) {
                                        cls.methods.push_back(node_text(mname, source));
                                    }
                                }
                            }
                        }
                    }
                }

                results.push_back(std::move(cls));
            }
        }
        return;
    }

    if (std::strcmp(type, profile.class_def_type().c_str()) == 0) {
        ASTNode cls;
        cls.type = NodeType::CLASS_DEF;
        cls.file_path = file_path;
        cls.start_line = ts_node_start_point(node).row + 1;
        cls.end_line = ts_node_end_point(node).row + 1;

        TSNode name_node = ts_node_child_by_field_name(
            node, profile.field_name().c_str(), profile.field_name().size()
        );
        if (!ts_node_is_null(name_node)) {
            cls.name = node_text(name_node, source);
        }

        // Base classes
        TSNode supers = ts_node_child_by_field_name(node, "superclasses", 12);
        if (!ts_node_is_null(supers)) {
            uint32_t sc = ts_node_named_child_count(supers);
            for (uint32_t j = 0; j < sc; j++) {
                cls.base_classes.push_back(
                    node_text(ts_node_named_child(supers, j), source)
                );
            }
        }

        // Method names from body
        TSNode body = ts_node_child_by_field_name(
            node, profile.field_body().c_str(), profile.field_body().size()
        );
        if (!ts_node_is_null(body)) {
            uint32_t bc = ts_node_child_count(body);
            for (uint32_t j = 0; j < bc; j++) {
                TSNode body_child = ts_node_child(body, j);
                const char* bc_type = ts_node_type(body_child);
                if (std::strcmp(bc_type, profile.function_def_type().c_str()) == 0) {
                    TSNode mname = ts_node_child_by_field_name(
                        body_child, profile.field_name().c_str(), profile.field_name().size()
                    );
                    if (!ts_node_is_null(mname)) {
                        cls.methods.push_back(node_text(mname, source));
                    }
                } else if (std::strcmp(bc_type, "decorated_definition") == 0) {
                    uint32_t dc = ts_node_child_count(body_child);
                    for (uint32_t k = 0; k < dc; k++) {
                        TSNode dd_child = ts_node_child(body_child, k);
                        if (std::strcmp(ts_node_type(dd_child), profile.function_def_type().c_str()) == 0) {
                            TSNode mname = ts_node_child_by_field_name(
                                dd_child, profile.field_name().c_str(), profile.field_name().size()
                            );
                            if (!ts_node_is_null(mname)) {
                                cls.methods.push_back(node_text(mname, source));
                            }
                        }
                    }
                }
            }

            // Also extract methods as FUNCTION_DEF nodes with parent_class set
            extract_functions(body, source, file_path, profile, results, cls.name);
        }

        results.push_back(std::move(cls));
        return;
    }

    // Recurse
    if (std::strcmp(type, "module") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            extract_classes(ts_node_child(node, i), source, file_path, profile, results);
        }
    }
}

void TSParserWrapper::extract_functions(TSNode node, const std::string& source,
                                         const std::string& file_path,
                                         const LanguageProfile& profile,
                                         std::vector<ASTNode>& results,
                                         const std::string& parent_class) {
    const char* type = ts_node_type(node);

    // Handle decorated definitions — extract decorators then process the inner def
    if (std::strcmp(type, "decorated_definition") == 0) {
        std::vector<std::string> decorators = extract_decorators(node, source, profile);

        // Find the actual definition inside
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            const char* child_type = ts_node_type(child);

            if (std::strcmp(child_type, profile.function_def_type().c_str()) == 0) {
                // Build function node
                ASTNode fn;
                fn.type = NodeType::FUNCTION_DEF;
                fn.file_path = file_path;
                fn.parent_class = parent_class;
                fn.start_line = ts_node_start_point(child).row + 1;
                fn.end_line = ts_node_end_point(child).row + 1;
                fn.decorators = decorators;

                TSNode name_node = ts_node_child_by_field_name(
                    child, profile.field_name().c_str(), profile.field_name().size()
                );
                if (!ts_node_is_null(name_node)) {
                    fn.name = node_text(name_node, source);
                }

                TSNode params = ts_node_child_by_field_name(
                    child, profile.field_parameters().c_str(), profile.field_parameters().size()
                );
                if (!ts_node_is_null(params)) {
                    fn.parameters = extract_parameters(params, source);
                }

                TSNode body = ts_node_child_by_field_name(
                    child, profile.field_body().c_str(), profile.field_body().size()
                );
                if (!ts_node_is_null(body)) {
                    collect_calls(body, source, profile, fn.called_functions);
                }

                results.push_back(std::move(fn));
            } else if (std::strcmp(child_type, profile.class_def_type().c_str()) == 0) {
                // Decorated class — skip here, handled by extract_classes
                continue;
            }
        }
        return;
    }

    // Handle function definitions
    if (std::strcmp(type, profile.function_def_type().c_str()) == 0) {
        ASTNode fn;
        fn.type = NodeType::FUNCTION_DEF;
        fn.file_path = file_path;
        fn.parent_class = parent_class;
        fn.start_line = ts_node_start_point(node).row + 1;
        fn.end_line = ts_node_end_point(node).row + 1;

        TSNode name_node = ts_node_child_by_field_name(
            node, profile.field_name().c_str(), profile.field_name().size()
        );
        if (!ts_node_is_null(name_node)) {
            fn.name = node_text(name_node, source);
        }

        TSNode params = ts_node_child_by_field_name(
            node, profile.field_parameters().c_str(), profile.field_parameters().size()
        );
        if (!ts_node_is_null(params)) {
            fn.parameters = extract_parameters(params, source);
        }

        TSNode body = ts_node_child_by_field_name(
            node, profile.field_body().c_str(), profile.field_body().size()
        );
        if (!ts_node_is_null(body)) {
            collect_calls(body, source, profile, fn.called_functions);
        }

        results.push_back(std::move(fn));
        return;
    }

    // Skip class definitions (handled by extract_classes)
    if (std::strcmp(type, profile.class_def_type().c_str()) == 0) return;

    // Recurse into children for top-level traversal
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        extract_functions(ts_node_child(node, i), source, file_path, profile, results, parent_class);
    }
}

} // namespace probe
