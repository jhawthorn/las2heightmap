FROM ubuntu:latest

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update -y \
  && apt-get install -y \
     software-properties-common \
  && add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable \
  && apt-get update -y

RUN apt-get install --no-install-recommends -y \
  build-essential \
  liblas-dev liblas-c-dev \
  libpng-dev libpng++-dev

COPY . /build/

ENTRYPOINT ["/build/las2heightmap"]
