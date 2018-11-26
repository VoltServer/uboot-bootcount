#!groovy
/* See: http://jenkins.voltserver.net/pipeline-syntax/globals
 * Use the following to validate:
 * ssh -p $(curl -I http://$JENKINS_HOST/api/ 2>&1 | grep 'X-SSH-Endpoint'|cut -d: -f3) \
 * admin@$JENKINS_HOST declarative_linter < Jenkinsfile
 */

pipeline {
  agent {
    label 'debian && gcc-arm'
  }
  triggers {
    pollSCM('H/4 * * * *')
  }
  stages {
    stage('Build') {
      environment {
        CONFIGURE_OPTS = "--host=arm-linux-gnueabihf --prefix=/usr"
      }
      steps {
        sh '[[ -f ./configure ]] || ./autogen.sh'
        sh '[[ -f Makefile ]] || ./configure "${CONFIGURE_OPTS}"'
        sh 'make'
        archiveArtifacts artifacts: 'src/bootcount'
      }
    }
  }
  post {
    failure {
      slackSend message: "Build Failed: <${env.RUN_DISPLAY_URL}|${env.JOB_NAME} #${env.BUILD_ID}>",
                color: 'danger', token: '--fake--', tokenCredentialId: 'slack-token'
    }
    success {
      slackSend message: "Build Success: <${env.RUN_DISPLAY_URL}|${env.JOB_NAME} #${env.BUILD_ID}>",
                color: 'good', token: '--fake--', tokenCredentialId: 'slack-token'
    }
  }
}
