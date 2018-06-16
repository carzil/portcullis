from portcullis import Splicer

class Handler:
    def __init__(self, ctx, client):
        self.ctx = ctx
        self.client = client
        self.ctx.connect("tcp://localhost:80", self.entry)

    def entry(self, backend):
        self.backend = backend
        self.splicer = Splicer(self.ctx, self.client, self.backend)
        self.splicer.start(on_end=self.end)

    def end(self):
        pass
