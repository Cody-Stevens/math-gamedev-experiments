// Export the reconstructed Snaky certificate (demos/03-snaky/snaky_cert.h) to compact JSON for the browser demo.
#include "../../../demos/03-snaky/snaky_cert.h"
#include <cstdio>
using namespace snaky;
int main(int argc, char** argv) {
    Certificate C;
    if (!C.load(argv[1])) { std::fprintf(stderr, "load failed: %s\n", C.err.c_str()); return 1; }
    FILE* f = std::fopen(argv[2], "wb");
    std::fprintf(f, "{\"sha_lf\":\"%s\",\"nodes\":[", C.sha_lf.c_str());
    for (size_t i = 0; i < C.nodes.size(); ++i) {
        const Node& n = C.nodes[i];
        // T as a list of cell indices (x*17+y), kids as [ref, idx, a, b, c, d, tx, ty]
        std::fprintf(f, "%s[%d,%d,%d,%d,[", i ? "," : "", n.px, n.py, n.base ? 1 : 0, n.h);
        bool first = true;
        n.T.each([&](int c) { std::fprintf(f, first ? "%d" : ",%d", c); first = false; });
        std::fprintf(f, "],[");
        for (size_t k = 0; k < n.kids.size(); ++k) {
            const Child& c = n.kids[k];
            std::fprintf(f, "%s[%d,%d,%d,%d,%d,%d,%d,%d]", k ? "," : "", c.ref ? 1 : 0, c.idx, c.g.a, c.g.b, c.g.c, c.g.d, c.g.tx, c.g.ty);
        }
        std::fprintf(f, "]]");
    }
    std::fprintf(f, "],\"card_node\":[");
    for (size_t i = 0; i < C.card_node.size(); ++i) std::fprintf(f, i ? ",%d" : "%d", C.card_node[i]);
    std::fprintf(f, "],\"t727\":[");
    for (size_t i = 0; i < C.t727_order.size(); ++i) std::fprintf(f, i ? ",%d" : "%d", C.t727_order[i]);
    std::fprintf(f, "]}\n");
    std::fclose(f);
    std::printf("nodes=%zu cards=%zu final h=%d sha=%s\n", C.nodes.size(), C.card_node.size(), C.nodes[C.card_node[727]].h, C.sha_lf.c_str());
}
