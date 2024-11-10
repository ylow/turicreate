project(Turi)

enable_testing()

subdirs(
        util
        serialization
        logger
        timer
        random
        nanosockets
        fileio
        shmipc
        cppipc
        flexible_type
        parallel
        lambda
        xframe
        lazy_eval
        xframe_query_engine
        unity
        sketches
        generics
        table_printer
        network
        process
        benchmark
        exceptions
        vega_renderer
)

