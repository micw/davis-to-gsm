FROM alpine:3.6

RUN apk add --update --no-cache python

ADD app /app

VOLUME /app/data

ENV API_KEY=secret
WORKDIR /app
EXPOSE 8080

CMD ./endpoint.py
