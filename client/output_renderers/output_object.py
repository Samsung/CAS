from client.output_renderers.output import OutputRenderer


class Renderer(OutputRenderer):
    help = 'This is meta renderer that actually returns object data (generated from output_renderers dir)'
    default_entries_count = 0

    @staticmethod
    def append_args(parser):
        pass

    def render_data(self):
        return self.data

    def count_renderer(self):
        return self.num_entries