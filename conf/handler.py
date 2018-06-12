class Handler:
    def __init__(self, ctx, client):
        self.ctx = ctx
        self.client = client
        self.conn = self.ctx.connect("tcp://localhost:80", self.entry)

    def entry(self, backend):
        self.backend = backend
        self.ctx.splice(self.client, self.backend)
